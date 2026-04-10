# DESIGN.md — Diseño Arquitectónico: Fases 1, 2 y 3 de `el_pedestal`

> **Alcance:** Este documento describe la arquitectura de software, las decisiones de ingeniería y los contratos de interfaz de las primitivas aritméticas (Fase 1), la NTT (Fase 2), y los contenedores de polinomios, vectores y algoritmos de compresión (Fase 3). Las demostraciones matemáticas rigurosas residen en [`docs/MATH_PROOFS.md`](docs/MATH_PROOFS.md).

---

## 1. Contexto del Proyecto

`el_pedestal` es una implementación bare-metal de **ML-DSA** (FIPS 204) en C99 para arquitecturas de 32 bits. La Fase 1 establece el núcleo aritmético sobre el cuerpo finito `Z_Q`, con `Q = 8 380 417`. Todas las primitivas deben cumplir:

- **Tiempo constante estricto:** ninguna operación de control de flujo (saltos, ramas) puede depender de valores secretos.
- **Portabilidad a 32 bits:** sin dependencias de instrucciones de 64 bits nativas; el tipo `int64_t` se usa solo en productos intermedios.
- **Sin bibliotecas de sistema:** código C99 puro, apto para entornos bare-metal/RTOS.

---

## 2. Parámetros del Cuerpo Finito

| Constante            | Valor       | Rol                                           |
|----------------------|-------------|-----------------------------------------------|
| `Q`                  | `8 380 417` | Módulo primo del cuerpo `Z_Q`                 |
| `QINV`               | `58 728 449`| Inverso de Q módulo 2³² para Montgomery       |
| `BARRETT_MULTIPLIER` | `8`         | Constante `m` de la reducción de Barrett (k=26) |

> Para la derivación matemática rigurosa de `QINV` (Lema de Hensel sobre 2-adics) y de `m = 8` (optimización de Barrett con k=26), consultar [`docs/MATH_PROOFS.md`](docs/MATH_PROOFS.md), demostraciones 2 y 4.

### 2.1 Por qué Q = 8 380 417

`Q` es el primo NTT estándar de ML-DSA (FIPS 204, §A). Satisface `Q ≡ 1 (mod 2⁸)`, lo que garantiza la existencia de raíces de la unidad de orden 256 necesarias para la NTT de longitud 256. Su representación cabe en 23 bits, lo que facilita la aritmética modular en registros de 32 bits.

---

## 3. Filosofía de Diseño: Código Branchless

### 3.1 Motivación — Ataques de Caché y Predicción de Saltos

En una implementación criptográfica de producción, las instrucciones de salto condicional (`if`, `?:`) son un vector de ataque si su predicado depende de material secreto:

- **Ataques de predicción de saltos (branch-prediction side-channels):** el estado del predictor de saltos de la CPU es observable por otros procesos mediante canales de tiempo. Un `if (x < Q)` cuyo resultado varía con la clave puede filtrar bits secretos.
- **Ataques de caché (cache-timing):** diferentes ramas ejecutan rutas de código distintas, lo que deja huellas en la caché de instrucciones (I-cache) observables por ataques como Flush+Reload.

### 3.2 Solución — Proyección de Máscara Aritmética

Sustituimos cada salto condicional por una secuencia de operaciones aritméticas y de bits cuyo grafo de ejecución es **idéntico** independientemente del valor de entrada. La técnica central es la **extensión de signo** (`x >> 31`):

- En C99 con tipos `int32_t`, el desplazamiento aritmético a la derecha propaga el bit de signo.
- Si `x < 0`, `x >> 31` produce `0xFFFFFFFF` (todos los bits a 1), que actúa como **máscara de selección activa**.
- Si `x ≥ 0`, `x >> 31` produce `0x00000000` (máscara nula).

La combinación `x + (Q & mask)` implementa así "suma Q si y solo si x es negativo" sin ningún salto.

> Para la demostración formal de la corrección de la extensión de signo en complemento a dos y su fundamentación en aritmética de tiempo constante, consultar [`docs/MATH_PROOFS.md`](docs/MATH_PROOFS.md), demostración 7.

---

## 4. Primitivas de Reducción Condicional

### 4.1 `conditional_subq(int32_t a)` — Substracción Condicional

**Archivo:** `src/arithmetic.c` · **Declaración:** `inc/arithmetic.h`

```c
int32_t conditional_subq(int32_t a) {
    int32_t res  = a - Q;
    int32_t mask = res >> 31;
    return res + (Q & mask);
}
```

**Contrato:**
- **Precondición:** `0 ≤ a < 2Q`
- **Postcondición:** `0 ≤ resultado < Q`

**Mecánica:** Se computa `res = a - Q`. Si `a < Q`, entonces `res < 0`, la máscara se activa y se restituye `Q`, devolviendo `a` intacto. Si `a ≥ Q`, `res ≥ 0`, la máscara es nula y se devuelve `a - Q`. Todo en tiempo constante.

---

### 4.2 `caddq(int32_t a)` — Suma Condicional

**Archivo:** `src/arithmetic.c` · **Declaración:** `inc/arithmetic.h`

```c
int32_t caddq(int32_t a) {
    int32_t mask = a >> 31;
    return a + (Q & mask);
}
```

**Contrato:**
- **Precondición:** `-Q < a < Q`
- **Postcondición:** `0 ≤ resultado < Q`

**Mecánica:** Si `a` es negativo, la máscara activa la suma de `Q`, llevando el resultado al rango `[0, Q)`. Si `a` ya es no negativo, la máscara es nula y el valor pasa sin modificar.

---

## 5. Reducción de Montgomery

**Archivo:** `src/arithmetic.c` · **Declaración:** `inc/arithmetic.h`

```c
int32_t montgomery_reduce(int64_t a) {
    int32_t t   = (int32_t)a * QINV;          // Fase 1: cómputo del factor corrector
    int32_t res = (int32_t)((a - (int64_t)t * Q) >> 32); // Fase 2: absorción y desplazamiento
    return res;
}
```

**Contrato:**
- **Entrada:** `a` de tipo `int64_t`, típicamente un producto `A·B` con `|A|, |B| < Q`
- **Salida:** `r` de tipo `int32_t` tal que `r ≡ a · 2⁻³² (mod Q)` y `|r| ≤ Q`

**Mecánica en dos fases:**

1. **Fase 1 — Factor corrector:** Se computa `t = (int32_t)a * QINV`. El truncamiento a 32 bits es intencional: trabaja con los 32 bits inferiores de `a`. El propósito de `t` es anular exactamente esos 32 bits inferiores en el paso siguiente.

2. **Fase 2 — Absorción y desplazamiento:** Se calcula `a - t·Q` en 64 bits. Gracias a la elección de `QINV = Q⁻¹ mod 2³²`, los 32 bits inferiores del resultado son siempre cero. El desplazamiento aritmético `>> 32` descarta esos bits nulos y extrae los 32 bits superiores como resultado `int32_t`.

> Para la demostración matemática de por qué `t·Q` anula exactamente los 32 bits inferiores (prueba de anulación), y la derivación de `QINV` por el Lema de Hensel, consultar [`docs/MATH_PROOFS.md`](docs/MATH_PROOFS.md), demostraciones 2 y 3.

---

## 6. Reducción de Barrett

**Archivo:** `src/arithmetic.c` · **Declaración:** `inc/arithmetic.h`

```c
int32_t barrett_reduce(int32_t a) {
    int32_t t   = (int32_t)(((int64_t)a * BARRETT_MULTIPLIER + (1 << 25)) >> 26);
    int32_t res = a - t * Q;
    return res;
}
```

**Contrato:**
- **Entrada:** `a` de tipo `int32_t`, con `|a| < 2³¹` (rango de 32 bits con signo)
- **Salida:** `r` de tipo `int32_t` tal que `r ≡ a (mod Q)` y `|r| ≤ Q/2`

**Mecánica en dos pasos:**

1. **Paso 1 — Estimación del cociente:** Se aproxima `t ≈ round(a / Q)` mediante la fórmula `(a · m + 2²⁵) >> 26`, donde `m = BARRETT_MULTIPLIER = 8` y `k = 26`. La inyección de `2²⁵ = 2^(k-1)` antes del desplazamiento implementa redondeo al entero más cercano (en lugar de truncamiento hacia cero), lo que confina el error residual al rango centrado `[-Q/2, Q/2]`.

2. **Paso 2 — Corrección residual:** Se computa `res = a - t·Q`. El cociente estimado puede diferir del exacto en ±1, por lo que el residuo puede caer en `(-3Q/2, 3Q/2)` antes de la corrección; con el redondeo correcto queda en `[-Q/2, Q/2]`.

> Para la derivación de `m = 8` (elección de `k = 26`), la demostración de la cota de error para entradas de 32 bits y la prueba de que `2²⁵` fuerza redondeo al entero más cercano y confina el residuo en `[-Q/2, Q/2]`, consultar [`docs/MATH_PROOFS.md`](docs/MATH_PROOFS.md), demostraciones 4, 5 y 6.

---

## 7. Interfaz Pública (`inc/arithmetic.h`)

```c
#ifndef ARITHMETIC_H
#define ARITHMETIC_H

#include <stdint.h>

/* Parámetros del cuerpo finito Z_Q */
#define Q                  8380417
#define QINV               58728449
#define BARRETT_MULTIPLIER 8

/* Reducción condicional — tiempo constante */
int32_t conditional_subq(int32_t a); // Precond: 0 ≤ a < 2Q
int32_t caddq(int32_t a);           // Precond: -Q < a < Q

/* Reducciones asimétricas */
int32_t montgomery_reduce(int64_t a); // Salida: a·2^-32 mod Q, |res| ≤ Q
int32_t barrett_reduce(int32_t a);    // Salida: a mod Q, |res| ≤ Q/2

#endif /* ARITHMETIC_H */
```

---

## 8. Decisiones de Ingeniería Relevantes

| Decisión | Alternativa Descartada | Razón |
|---|---|---|
| Máscaras aritméticas con `>> 31` | `if (x < 0) x += Q` | Los saltos son observables por canales de tiempo |
| `int32_t` / `int64_t` explícitos | `int` / `long` | Portabilidad estricta entre ILP32 y LP64 |
| Sin `__builtin_*` ni intrínsecas | Intrínsecas SIMD de GCC | Portabilidad bare-metal; sin ABI de compilador |
| Barrett sobre entradas de 32 bits | Montgomery para todo | Montgomery requiere factor de escala; Barrett es directo para coeficientes individuales |
| Salida de Barrett en `[-Q/2, Q/2]` | Salida en `[0, Q)` | Permite aritmética acumulativa antes de reducción final |

---

---

# Fase 2: NTT (Number Theoretic Transform)

---

## 9. Contexto de la NTT en ML-DSA

ML-DSA opera sobre polinomios en el anillo `Z_Q[X] / (X^256 + 1)`. La multiplicación de polinomios en este anillo es la operación más costosa del algoritmo de firma. La **NTT** (Number Theoretic Transform) reduce la complejidad de esta multiplicación de `O(N²)` a `O(N log N)`, donde `N = 256`.

La NTT es el equivalente modular de la FFT (Fast Fourier Transform): transforma un polinomio del dominio de coeficientes al dominio de evaluación, donde la multiplicación de polinomios se reduce a 256 multiplicaciones escalares independientes (pointwise).

---

## 10. Parámetros de la NTT

| Constante | Valor       | Rol                                                              |
|-----------|-------------|------------------------------------------------------------------|
| `N`       | `256`       | Grado del polinomio / longitud de la NTT                        |
| `ZETA`    | `1753`      | Raíz primitiva 512-ésima de la unidad en `Z_Q`                  |
| `f`       | `41978`     | Factor de normalización de la INTT: `f = R² · N⁻¹ mod Q`       |
| `zetas[]` | 256 valores | Tabla precalculada de potencias de `ZETA` en dominio Montgomery  |

> Para la derivación de `ZETA = 1753`, la justificación del uso de raíces de orden 512 (no 256), la permutación bit-reversal de la tabla `zetas[]`, y la derivación de `f = 41978`, consultar [`docs/MATH_PROOFS.md`](docs/MATH_PROOFS.md), demostraciones 8–12.

### 10.1 Por qué ZETA = 1753 y orden 512

La NTT que necesita ML-DSA opera en el anillo cociente `Z_Q[X] / (X^256 + 1)`, no en `Z_Q[X] / (X^256 - 1)`. El factor `(X^256 + 1)` exige una raíz de la unidad de **orden 512** (no 256), ya que necesitamos `w` tal que `w^256 ≡ -1 (mod Q)`. Esto solo es posible si `w` tiene orden exactamente 512.

`ZETA = 1753` es la raíz primitiva 512-ésima de la unidad en `Z_Q` usada por la implementación de referencia de ML-DSA.

---

## 11. La Tabla de Zetas: Precomputación y Bit-Reversal

**Archivo:** `src/ntt.c` · **Generador:** `tools/generate_zetas.py`

```c
extern const int32_t zetas[256];
```

La tabla `zetas[]` contiene las 256 potencias de `ZETA` que la NTT necesita, con dos transformaciones aplicadas:

1. **Permutación bit-reversal:** El índice `i` de la tabla almacena `ZETA^(bitrev(i))`, donde `bitrev` invierte los 8 bits del índice. Esto permite que los tres bucles anidados de la NTT accedan a las zetas de forma **secuencial** (`k++`), eliminando la necesidad de calcular índices complejos en tiempo de ejecución.

2. **Conversión al dominio de Montgomery:** Cada valor se almacena como `ZETA^(bitrev(i)) · 2³² mod Q`. Esto permite que la operación butterfly use directamente `montgomery_reduce((int64_t)zeta * a[j+len])` sin conversiones adicionales.

### 11.1 Generación (`tools/generate_zetas.py`)

```python
for i in range(256):
    br_i = bit_reverse(i)                  # Paso 1: Permutar índice
    z_power = pow(ZETA, br_i, Q)            # Paso 2: Potencia modular
    z_mont = (z_power * (1 << 32)) % Q      # Paso 3: Al dominio Montgomery
```

---

## 12. NTT Directa (`poly_ntt`)

**Archivo:** `src/ntt.c` · **Declaración:** `inc/ntt.h`

```c
void poly_ntt(int32_t a[256]);
```

**Contrato:**
- **Entrada:** Array de 256 coeficientes en `Z_Q` (dominio normal o parcialmente reducido)
- **Salida:** Array de 256 coeficientes en el **dominio NTT** (dominio de evaluación, en representación de Montgomery)
- **In-place:** Modifica el array de entrada directamente

**Estructura de tres bucles anidados (Cooley-Tukey, decimación en tiempo):**

```c
void poly_ntt(int32_t a[256]) {
    unsigned int len, start, j, k;
    int32_t zeta, t;

    k = 1;
    for (len = 128; len >= 1; len >>= 1) {          // Bucle 1: 8 capas
        for (start = 0; start < 256; start = j + len) { // Bucle 2: bloques
            zeta = zetas[k++];
            for (j = start; j < start + len; ++j) {    // Bucle 3: butterfly
                t = montgomery_reduce((int64_t)zeta * a[j + len]);
                a[j + len] = a[j] - t;
                a[j]       = a[j] + t;
            }
        }
    }
}
```

**Mecánica de la mariposa (butterfly):**

1. `t = montgomery_reduce((int64_t)zeta * a[j + len])` — Multiplica el elemento "inferior" por la zeta y reduce.
2. `a[j + len] = a[j] - t` — El nuevo elemento inferior es la diferencia.
3. `a[j] = a[j] + t` — El nuevo elemento superior es la suma.

**Acceso secuencial a zetas:** El índice `k` avanza linealmente (`k++`). La permutación bit-reversal de la tabla garantiza que esto sea correcto sin cálculos de índice adicionales.

---

## 13. NTT Inversa (`poly_invntt`)

**Archivo:** `src/ntt.c` · **Declaración:** `inc/ntt.h`

```c
void poly_invntt(int32_t a[256]);
```

**Contrato:**
- **Entrada:** Array de 256 coeficientes en el dominio NTT
- **Salida:** Array de 256 coeficientes en el dominio normal (coeficientes del polinomio)
- **In-place:** Modifica el array de entrada directamente

**Estructura (Gentleman-Sande, decimación en frecuencia):**

```c
void poly_invntt(int32_t a[256]) {
    unsigned int len, start, j, k;
    int32_t zeta, t;

    k = 255;
    for (len = 1; len < 256; len <<= 1) {              // Bucle 1: 8 capas (invertido)
        for (start = 0; start < 256; start = j + len) {
            zeta = -zetas[k--];                         // Zeta negada, índice descendente
            for (j = start; j < start + len; ++j) {
                t = a[j];
                a[j]       = caddq(t + a[j + len]);    // Suma + normalización
                a[j + len] = t - a[j + len];
                a[j + len] = montgomery_reduce((int64_t)zeta * a[j + len]);
            }
        }
    }

    const int32_t f = 41978;                            // Factor de normalización
    for (j = 0; j < 256; j++) {
        a[j] = montgomery_reduce((int64_t)a[j] * f);
    }
}
```

**Diferencias clave respecto a `poly_ntt`:**

| Aspecto              | `poly_ntt` (directa)        | `poly_invntt` (inversa)                  |
|----------------------|-----------------------------|-------------------------------------------|
| Dirección de `len`   | `128 → 1` (divide por 2)   | `1 → 128` (multiplica por 2)             |
| Dirección de `k`     | `1 → 255` (ascendente)     | `255 → 1` (descendente)                  |
| Signo de zeta        | `+zetas[k]`                | `-zetas[k]` (conjugado)                  |
| Orden de la butterfly| Multiplica antes de sumar/restar | Suma/resta antes de multiplicar     |
| Normalización final  | No necesaria               | Multiplicación por `f = 41978`            |
| Uso de `caddq`       | No                         | Sí (en la suma de la butterfly inversa)   |

### 13.1 El factor de normalización `f = 41978`

El valor `f = 41978` incorpora dos correcciones en una sola multiplicación:

1. **El factor `1/N = 1/256`:** La NTT inversa necesita dividir por `N` para deshacer la escala de la NTT directa.
2. **La des-conversión de Montgomery (`R²`):** Los coeficientes acumulan factores de Montgomery durante las 7+1 capas de `montgomery_reduce`. Para cancelarlos, `f` incluye `R² = (2³²)² mod Q`.

$$f = N^{-1} \cdot R^2 \pmod{Q} = 8347681 \cdot 2365951 \pmod{8380417} = 41978$$

---

## 14. Multiplicación Pointwise (`poly_mul_pointwise`)

**Archivo:** `src/ntt.c` · **Declaración:** `inc/ntt.h`

```c
void poly_mul_pointwise(int32_t c[256], const int32_t a[256], const int32_t b[256]);
```

**Contrato:**
- **Entrada:** `a` y `b` en dominio NTT
- **Salida:** `c` en dominio NTT, donde `c[i] = a[i] · b[i] · 2⁻³² mod Q`

**Mecánica:** 256 multiplicaciones escalares independientes, cada una seguida de `montgomery_reduce`:

```c
for (i = 0; i < 256; ++i) {
    c[i] = montgomery_reduce((int64_t)a[i] * b[i]);
}
```

Esta es la razón de ser de la NTT: la multiplicación de polinomios de complejidad `O(N²)` en el dominio de coeficientes se convierte en `O(N)` multiplicaciones independientes en el dominio de evaluación.

---

## 15. Interfaz Pública de Fase 2 (`inc/ntt.h`)

```c
#ifndef NTT_H
#define NTT_H

#include <stdint.h>

extern const int32_t zetas[256];

void poly_ntt(int32_t a[256]);
void poly_mul_pointwise(int32_t c[256], const int32_t a[256], const int32_t b[256]);
void poly_invntt(int32_t a[256]);

#endif
```

---

## 16. Decisiones de Ingeniería — Fase 2

| Decisión | Alternativa Descartada | Razón |
|---|---|---|
| Zetas en bit-reversal + dominio Montgomery | Cálculo dinámico de potencias de ζ | Acceso secuencial O(1), sin exponenciación modular en runtime |
| NTT in-place (Cooley-Tukey / Gentleman-Sande) | NTT con buffer auxiliar | Ahorro de 1 KiB de RAM (crítico en embedded) |
| Factor `f` unificado en la INTT | Normalización separada `1/N` + des-Montgomery | Una sola pasada final reduce el número de `montgomery_reduce` |
| `caddq` en la butterfly inversa | Sin normalización intermedia | Evita crecimiento descontrolado de los coeficientes entre capas |
| Tabla `zetas[]` en `const` (Flash) | Tabla en RAM | Los 1 KiB de zetas residen en ROM/Flash, no consumen RAM |

---

---

# Fase 3: Polinomios, Vectores y Algoritmos de Compresión

---

## 18. Topología de Memoria y Estructuras de Datos

### 18.1 El tipo `poly`: Del array desnudo al contenedor formal

En las Fases 1 y 2, un polinomio de $\mathbb{Z}_Q[X]/(X^{256}+1)$ era simplemente un array `int32_t a[256]` pasado por puntero. Eso era aceptable mientras solo existían funciones aisladas (`poly_ntt`, `poly_invntt`), pero a partir de la Fase 3, donde las funciones reciben y devuelven *conjuntos* de polinomios, la ausencia de un tipo formal genera tres problemas:

1. **Ambigüedad de tamaño:** un parámetro `int32_t *a` no porta información de longitud. En funciones que operan sobre vectores de polinomios, confundir un puntero a un solo polinomio con un puntero a un vector de ellos es un error silencioso e indetectable por el compilador.
2. **Imposibilidad de paso por valor:** las funciones no pueden devolver un polinomio por valor si éste es un array desnudo (los arrays se degradan a punteros en C).
3. **Alineación y padding:** encapsular el array en un `struct` permite al compilador aplicar reglas de alineación específicas del target (ej. alineación a 4 bytes en Cortex-M4) de forma predecible.

**Archivo:** `inc/poly.h`

```c
#ifndef POLY_H
#define POLY_H

#include <stdint.h>

#define N 256

typedef struct {
    int32_t coeffs[N];
} poly;

#endif /* POLY_H */
```

El `struct` no añade ningún byte de overhead frente al array desnudo (el compilador no inserta padding entre un único miembro), pero sí habilita pasar polinomios por puntero con tipado fuerte. A partir de la Fase 3, todas las funciones aceptan `poly *` en lugar de `int32_t *`.

> **Nota de migración:** Las funciones de Fase 2 (`poly_ntt`, `poly_invntt`, `poly_mul_pointwise`) deberán adaptar sus firmas para aceptar `poly *` en lugar de `int32_t[256]`. El cambio es mecánico: sustituir `a[i]` por `a->coeffs[i]`.

---

### 18.2 Vectores de Polinomios: `polyvecl` y `polyveck`

ML-DSA opera sobre vectores y matrices de polinomios cuyas dimensiones dependen del nivel de seguridad. FIPS 204, Tabla 1, define tres conjuntos de parámetros:

| Parámetro     | ML-DSA-44 | ML-DSA-65 | ML-DSA-87 | Rol en el esquema                                      |
|---------------|-----------|-----------|-----------|--------------------------------------------------------|
| $k$           | 4         | 6         | 8         | Filas de la matriz $\mathbf{A}$; longitud del vector $\mathbf{t}$ |
| $\ell$        | 4         | 5         | 7         | Columnas de $\mathbf{A}$; longitud del vector $\mathbf{s}_1$      |
| $\eta$        | 2         | 4         | 2         | Cota de los coeficientes de las claves secretas        |
| $d$           | 13        | 13        | 13        | Bits descartados por `Power2Round`                     |
| $\gamma_1$    | $2^{17}$  | $2^{19}$  | $2^{19}$  | Rango del muestreo del enmascaramiento $\mathbf{y}$    |
| $\gamma_2$    | 95 232    | 261 888   | 261 888   | Divisor de descomposición: $(Q-1)/88$ o $(Q-1)/32$     |
| $\tau$        | 39        | 49        | 60        | Peso del polinomio de desafío $c$                      |
| $\beta$       | 78        | 196       | 120       | Cota de rechazo: $\tau \cdot \eta$                     |
| $\omega$      | 80        | 55        | 75        | Máximo de coeficientes no nulos en el vector hint $\mathbf{h}$ |

Los valores de $\gamma_2$ se derivan directamente del módulo:

$$\gamma_2 = \frac{Q - 1}{88} = \frac{8\,380\,416}{88} = 95\,232 \quad \text{(ML-DSA-44)}$$

$$\gamma_2 = \frac{Q - 1}{32} = \frac{8\,380\,416}{32} = 261\,888 \quad \text{(ML-DSA-65, ML-DSA-87)}$$

La elección de un nivel de seguridad concreto se realiza en tiempo de compilación mediante un `#define`. Las dimensiones $k$ y $\ell$ determinan los tipos vectoriales:

**Archivo:** `inc/poly.h` (continuación)

```c
/* ================================================================
 * Selección del nivel de seguridad en tiempo de compilación.
 * Descomentar exactamente una línea, o definir desde el Makefile
 * con -DDILITHIUM_MODE=N.
 * ================================================================ */
#ifndef DILITHIUM_MODE
#define DILITHIUM_MODE 2   /* Por defecto: ML-DSA-44 */
#endif

#if DILITHIUM_MODE == 2       /* ML-DSA-44 */
#define K         4
#define L         4
#define ETA       2
#define TAU       39
#define BETA      78
#define GAMMA1    (1 << 17)
#define GAMMA2    ((Q - 1) / 88)
#define OMEGA     80
#elif DILITHIUM_MODE == 3     /* ML-DSA-65 */
#define K         6
#define L         5
#define ETA       4
#define TAU       49
#define BETA      196
#define GAMMA1    (1 << 19)
#define GAMMA2    ((Q - 1) / 32)
#define OMEGA     55
#elif DILITHIUM_MODE == 5     /* ML-DSA-87 */
#define K         8
#define L         7
#define ETA       2
#define TAU       60
#define BETA      120
#define GAMMA1    (1 << 19)
#define GAMMA2    ((Q - 1) / 32)
#define OMEGA     75
#else
#error "DILITHIUM_MODE debe ser 2, 3, o 5"
#endif

#define D 13   /* Bits descartados por Power2Round (fijo en FIPS 204) */

typedef struct {
    poly vec[L];
} polyvecl;

typedef struct {
    poly vec[K];
} polyveck;
```

El mapa de memoria resultante para cada tipo, suponiendo `sizeof(int32_t) = 4`:

```
┌──────────────────────────────────────────────────────────┐
│  poly:     256 coeficientes × 4 bytes = 1 024 bytes      │
├──────────────────────────────────────────────────────────┤
│  polyvecl: L polinomios × 1 024 bytes                    │
│            ML-DSA-44: 4 × 1 024 =  4 096 bytes (4 KiB)  │
│            ML-DSA-65: 5 × 1 024 =  5 120 bytes (5 KiB)  │
│            ML-DSA-87: 7 × 1 024 =  7 168 bytes (7 KiB)  │
├──────────────────────────────────────────────────────────┤
│  polyveck: K polinomios × 1 024 bytes                    │
│            ML-DSA-44: 4 × 1 024 =  4 096 bytes (4 KiB)  │
│            ML-DSA-65: 6 × 1 024 =  6 144 bytes (6 KiB)  │
│            ML-DSA-87: 8 × 1 024 =  8 192 bytes (8 KiB)  │
└──────────────────────────────────────────────────────────┘
```

---

### 18.3 Huella de Memoria en RAM y Estrategia de Stack

En un microcontrolador de 32 bits típico (ej. STM32F4 con 128 KiB de SRAM), la firma digital ML-DSA necesita tener en vuelo simultáneamente, como mínimo, las siguientes estructuras durante la operación `Sign`:

| Variable            | Tipo        | Tamaño (ML-DSA-44) | Tamaño (ML-DSA-87) |
|---------------------|-------------|---------------------|---------------------|
| $\mathbf{s}_1$      | `polyvecl`  | 4 KiB               | 7 KiB               |
| $\mathbf{s}_2$      | `polyveck`  | 4 KiB               | 8 KiB               |
| $\mathbf{t}_0$      | `polyveck`  | 4 KiB               | 8 KiB               |
| $\mathbf{y}$        | `polyvecl`  | 4 KiB               | 7 KiB               |
| $\mathbf{w}$        | `polyveck`  | 4 KiB               | 8 KiB               |
| $\mathbf{z}$        | `polyvecl`  | 4 KiB               | 7 KiB               |
| $\mathbf{h}$        | `polyveck`  | 4 KiB               | 8 KiB               |
| **Total aproximado** |            | **~28 KiB**         | **~53 KiB**         |

Estas cifras excluyen la matriz $\mathbf{A}$ (que se regenera *on-the-fly* mediante `ExpandA` en Fase 4 para no almacenarla), los buffers de hash, y el overhead del stack de llamadas.

**Restricciones de diseño derivadas:**

1. **Sin `malloc`:** toda la memoria se declara como variables locales (stack) o como `static` (BSS). No existe heap en un entorno bare-metal típico.
2. **Reutilización de buffers:** las variables cuya vida útil no se solapa se declaran en el mismo ámbito de stack para que el compilador pueda reutilizar la misma región de memoria. Por ejemplo, $\mathbf{y}$ y $\mathbf{z}$ nunca coexisten (el cálculo de $\mathbf{z}$ sobreescribe $\mathbf{y}$).
3. **Fila a fila para $\mathbf{A}$:** la matriz $\mathbf{A}$ ($k \times \ell$ polinomios = 16 KiB para ML-DSA-44) nunca se materializa completa. Se genera una fila de $\ell$ polinomios, se usa para acumular el producto interno contra $\mathbf{s}_1$, y se descarta antes de generar la siguiente fila. Coste: 1 `polyvecl` temporal ($\ell$ KiB) en lugar de $k \times \ell$ KiB.

---

## 19. Aritmética Vectorial Escalada

### 19.1 Propagación de la Transformada: NTT sobre vectores

Cada `polyvecl` o `polyveck` es un array de polinomios independientes. Transformar un vector al dominio NTT consiste simplemente en iterar `poly_ntt` sobre cada componente:

**Archivo:** `src/polyvec.c`

```c
void polyvecl_ntt(polyvecl *v) {
    unsigned int i;
    for (i = 0; i < L; ++i)
        poly_ntt(v->vec[i].coeffs);
}

void polyveck_ntt(polyveck *v) {
    unsigned int i;
    for (i = 0; i < K; ++i)
        poly_ntt(v->vec[i].coeffs);
}
```

Las funciones análogas `polyvecl_invntt` y `polyveck_invntt` invocan `poly_invntt` del mismo modo. La operación no tiene interacción entre las componentes del vector: cada polinomio se transforma independientemente.

---

### 19.2 Multiplicación de Vectores: Producto Interno

El producto de la matriz $\mathbf{A}$ (de dimensión $k \times \ell$) por el vector $\mathbf{s}$ (de dimensión $\ell$) produce un vector de dimensión $k$. Cada componente $i$ del resultado es el producto interno:

$$t_i = \sum_{j=0}^{\ell - 1} A_{ij} \cdot s_j \quad (\text{en dominio NTT})$$

En el dominio NTT, la multiplicación de polinomios es pointwise (256 multiplicaciones escalares independientes), y la suma de polinomios es coeficiente a coeficiente. El producto interno se acumula en un polinomio acumulador.

**Archivo:** `src/polyvec.c`

```c
void polyvecl_pointwise_acc(poly *r, const poly *a, const polyvecl *b) {
    unsigned int i;
    poly t;

    poly_mul_pointwise(r->coeffs, a[0].coeffs, b->vec[0].coeffs);
    for (i = 1; i < L; ++i) {
        poly_mul_pointwise(t.coeffs, a[i].coeffs, b->vec[i].coeffs);
        poly_add(r, r, &t);
    }
}
```

Donde `poly_add` es la suma coeficiente a coeficiente:

```c
void poly_add(poly *r, const poly *a, const poly *b) {
    unsigned int i;
    for (i = 0; i < N; ++i)
        r->coeffs[i] = a->coeffs[i] + b->coeffs[i];
}
```

**Control de desbordamiento en la acumulación.** La suma de $\ell$ productos pointwise puede crecer. Tras cada `montgomery_reduce` dentro de `poly_mul_pointwise`, el resultado por coeficiente satisface $|r_j| \leq Q$. Al sumar $\ell$ de estos valores:

$$|a_{\text{acum}}| \leq \ell \cdot Q$$

Para ML-DSA-87 ($\ell = 7$): $7 \times 8\,380\,417 = 58\,662\,919 < 2^{26}$. Esto cabe holgadamente en un `int32_t` ($< 2^{31}$). No se requiere reducción intermedia entre sumas. Tras la acumulación completa, una sola llamada a `barrett_reduce` por coeficiente normaliza el resultado.

```c
void poly_reduce(poly *a) {
    unsigned int i;
    for (i = 0; i < N; ++i)
        a->coeffs[i] = barrett_reduce(a->coeffs[i]);
}
```

**Funciones auxiliares adicionales:**

```c
void poly_sub(poly *r, const poly *a, const poly *b) {
    unsigned int i;
    for (i = 0; i < N; ++i)
        r->coeffs[i] = a->coeffs[i] - b->coeffs[i];
}

void poly_caddq(poly *a) {
    unsigned int i;
    for (i = 0; i < N; ++i)
        a->coeffs[i] = caddq(a->coeffs[i]);
}
```

---

## 20. Algoritmos de Descomposición y Compresión

### 20.1 Fundamento Teórico: Por qué comprimir

En ML-DSA, la clave pública contiene un vector $\mathbf{t}$ de $k$ polinomios con coeficientes en $\mathbb{Z}_Q$. Serializar cada coeficiente completo requeriría 23 bits por coeficiente × 256 coeficientes × $k$ polinomios. Para ML-DSA-44:

$$23 \times 256 \times 4 = 23\,552 \text{ bits} = 2\,944 \text{ bytes solo para } \mathbf{t}$$

La compresión reduce esta huella. La idea central es dividir cada coeficiente $r$ en una parte alta $r_1$ (que retiene la mayor parte de la información) y una parte baja $r_0$ (que se descarta o almacena por separado). Existen dos mecanismos de división, adaptados a dos contextos distintos dentro del algoritmo:

| Mecanismo      | Divisor        | Contexto de uso                | Referencia FIPS 204     |
|----------------|----------------|--------------------------------|-------------------------|
| `Power2Round`  | $2^d$ ($d=13$) | Separar $\mathbf{t}$ en $\mathbf{t}_1$ y $\mathbf{t}_0$ durante `KeyGen` | Algoritmo 35 |
| `Decompose`    | $2\gamma_2$    | Extraer los bits altos $\mathbf{w}_1$ del compromiso $\mathbf{w}$ durante `Sign` y `Verify` | Algoritmo 36 |

---

### 20.2 `Power2Round`: División binaria en $r_1$ y $r_0$

**Definición (FIPS 204, Algoritmo 35):** Dado $r \in \mathbb{Z}_Q$ y el parámetro fijo $d = 13$:

1. $r^+ \leftarrow r \bmod Q$ (reducción al rango canónico $[0, Q)$)
2. $r_0 \leftarrow r^+ \bmod^{\pm} 2^d$ (residuo centrado en $(-2^{d-1}, 2^{d-1}]$)
3. $r_1 \leftarrow (r^+ - r_0) / 2^d$

La notación $\bmod^{\pm}$ denota el residuo centrado: si $r^+ \bmod 2^d > 2^{d-1}$, se resta $2^d$ para obtener un valor negativo. Esto garantiza $r_0 \in (-2^{12}, 2^{12}]$, es decir, $|r_0| \leq 4\,096$.

**Mapa de bits de un coeficiente tras `Power2Round`:**

```
Coeficiente r (23 bits útiles):
┌────────────────────────────┬───────────────┐
│       r₁ (10 bits)         │  r₀ (13 bits) │
│  bits [22..13]             │  bits [12..0]  │
│  Se almacena en t₁ (PK)   │  Se almacena   │
│                            │  en t₀ (SK)    │
└────────────────────────────┴───────────────┘
          ↑                         ↑
     Clave pública             Clave privada
```

El rango de $r_1$: dado $r^+ \in [0, Q)$ y $|r_0| \leq 2^{12}$:

$$r_1 = \frac{r^+ - r_0}{2^{13}} \in \left[0,\; \frac{Q - 1 + 2^{12}}{2^{13}}\right] = [0,\; 1\,023]$$

Por tanto $r_1$ cabe en 10 bits. La clave pública $\mathbf{t}_1$ ocupa $10 \times 256 \times k$ bits en lugar de $23 \times 256 \times k$: una reducción del 56%.

**Implementación:**

```c
void poly_power2round(poly *r1, poly *r0, const poly *a) {
    unsigned int i;
    for (i = 0; i < N; ++i)
        power2round(&r1->coeffs[i], &r0->coeffs[i], a->coeffs[i]);
}
```

Donde la función escalar es:

```c
void power2round(int32_t *r1, int32_t *r0, int32_t a) {
    int32_t a_pos = caddq(a);              /* a⁺ ∈ [0, Q) */

    *r0 = a_pos - ((a_pos + (1 << (D - 1))) >> D) * (1 << D);
    /* Equivale a: r0 = a⁺ mod±(2^D)
     *
     * Desglose:
     * (a_pos + 2^12) >> 13  →  round(a_pos / 2^13) = r1
     * r1 * 2^13             →  reconstrucción parcial
     * a_pos - (r1 * 2^13)   →  r0 = a⁺ - r1·2^D
     */

    *r1 = (a_pos - *r0) >> D;
}
```

**Contrato:**
- **Precondición:** `a` ∈ $(-Q, Q)$ (salida de `barrett_reduce` o similar)
- **Postcondición:** `r1` ∈ $[0, 1\,023]$, `r0` ∈ $(-2^{12}, 2^{12}]$, y $a \equiv r_1 \cdot 2^{13} + r_0 \pmod{Q}$

---

### 20.3 `Decompose`, `HighBits` y `LowBits`: División respecto a $\alpha = 2\gamma_2$

Mientras `Power2Round` divide con un divisor potencia de 2, `Decompose` divide respecto al parámetro $\alpha = 2\gamma_2$, que depende del nivel de seguridad. Este divisor no es potencia de 2, así que la aritmética es más delicada.

**Definición (FIPS 204, Algoritmo 36):** Dado $r \in \mathbb{Z}_Q$ y $\alpha = 2\gamma_2$:

1. $r^+ \leftarrow r \bmod Q$
2. $r_0 \leftarrow r^+ \bmod^{\pm} \alpha$
3. Si $r^+ - r_0 = Q - 1$: fijar $r_1 = 0$, $r_0 = r_0 - 1$
4. Si no: $r_1 = (r^+ - r_0) / \alpha$

El paso 3 es un caso frontera (*corner case*) que se activa cuando $r^+$ está tan cerca de $Q - 1$ que la sustracción del residuo cruza el límite modular. En ese caso, el cociente se fuerza a 0 y el residuo absorbe la corrección.

**Rango de $r_1$** (fuera del corner case):

$$r_1 \in \left[0,\; \frac{Q - 1}{\alpha}\right)$$

Para ML-DSA-44: $r_1 \in [0, 43]$, pues $(Q-1)/(2 \times 95\,232) = 8\,380\,416 / 190\,464 = 44$, y el rango es $[0, 43]$ con el corner case mapeando el valor 44 a 0.

Para ML-DSA-65/87: $r_1 \in [0, 15]$, pues $(Q-1)/(2 \times 261\,888) = 8\,380\,416 / 523\,776 = 16$, y el corner case mapea 16 a 0.

**Implementación:**

```c
void decompose(int32_t *r1, int32_t *r0, int32_t a) {
    int32_t a_pos = caddq(a);

    /* r0 = a⁺ mod±(α) */
    *r0 = a_pos % (2 * GAMMA2);                       /* residuo positivo */
    if (*r0 > GAMMA2)
        *r0 -= 2 * GAMMA2;                            /* centrar */

    /* Corner case: a⁺ - r0 = Q - 1 */
    if (a_pos - *r0 == Q - 1) {
        *r1 = 0;
        *r0 = *r0 - 1;
    } else {
        *r1 = (a_pos - *r0) / (2 * GAMMA2);
    }
}
```

> **Nota sobre tiempo constante.** La función `decompose` contiene ramas condicionales (`if`). Esto es aceptable porque sus argumentos no son valores secretos: `decompose` se aplica sobre el compromiso $\mathbf{w} = \mathbf{A} \cdot \mathbf{y}$, que es público (se envía como parte de la firma o se recalcula durante la verificación). Las ramas no filtran información sobre la clave privada.
>
> La operación `%` (módulo genérico) se usa aquí porque $\alpha$ no es potencia de 2, impidiendo el truco de Barrett con shifts. Dado que esta función no opera sobre datos secretos, el coste de la instrucción de división es tolerable.

`HighBits` y `LowBits` son envoltorios triviales:

```c
int32_t highbits(int32_t a) {
    int32_t r1, r0;
    decompose(&r1, &r0, a);
    return r1;
}

int32_t lowbits(int32_t a) {
    int32_t r1, r0;
    decompose(&r1, &r0, a);
    return r0;
}
```

Y sus extensiones polinómicas:

```c
void poly_decompose(poly *r1, poly *r0, const poly *a) {
    unsigned int i;
    for (i = 0; i < N; ++i)
        decompose(&r1->coeffs[i], &r0->coeffs[i], a->coeffs[i]);
}

void poly_highbits(poly *r1, const poly *a) {
    unsigned int i;
    for (i = 0; i < N; ++i)
        r1->coeffs[i] = highbits(a->coeffs[i]);
}

void poly_lowbits(poly *r0, const poly *a) {
    unsigned int i;
    for (i = 0; i < N; ++i)
        r0->coeffs[i] = lowbits(a->coeffs[i]);
}
```

---

## 21. El Mecanismo de Corrección: Hints

### 21.1 El Problema de la Frontera

El esquema de firma ML-DSA sigue la estrategia *Fiat-Shamir with Aborts*. Durante la firma, el firmante calcula un compromiso $\mathbf{w} = \mathbf{A} \cdot \mathbf{y}$ y extrae sus bits altos $\mathbf{w}_1 = \text{HighBits}(\mathbf{w})$. El verificador, que no conoce $\mathbf{w}$ directamente, debe reconstruir $\mathbf{w}_1$ a partir de la firma recibida.

El verificador calcula un valor derivado $\mathbf{w}' = \mathbf{A} \cdot \mathbf{z} - c \cdot \mathbf{t} \cdot 2^d$, que difiere de $\mathbf{w}$ por una cantidad pequeña (controlada por los parámetros de rechazo). El problema surge cuando esa diferencia, aunque pequeña en magnitud absoluta, provoca un cambio en los bits altos — un **acarreo** (carry) en la frontera entre regiones de $\alpha$.

Visualización del fenómeno:

```
Eje de Z_Q (rango [0, Q)):
                                              
├─────────┤─────────┤─────────┤─────────┤───
   r₁=0      r₁=1      r₁=2      r₁=3    ...
                                              
         α        2α        3α               
                                              
Caso normal:                                  
    w = ●────────── (bien centrado en la región r₁=1)
    w'= ○────────── (ligeramente desplazado, sigue en r₁=1)
    → HighBits(w) = HighBits(w') = 1.  Sin carry.
                                              
Caso frontera:                                
    w = ──────────● (cerca del borde entre r₁=1 y r₁=2)
    w'= ──────────────○ (cruzó la frontera a r₁=2)
    → HighBits(w) = 1,  HighBits(w') = 2.  ¡Carry!
```

Si no se corrige este carry, el verificador calculará $\mathbf{w}'_1 \neq \mathbf{w}_1$, el hash del compromiso será distinto, y la verificación fallará a pesar de que la firma es legítima.

---

### 21.2 `MakeHint`: Generación de la máscara booleana

El firmante, que conoce tanto $\mathbf{w}$ como la perturbación que introduce su firma, genera un vector de *hints* (pistas) $\mathbf{h}$ que indica, para cada coeficiente, si se produjo un carry en los bits altos.

**Definición (FIPS 204, Algoritmo 39):**

```
MakeHint(z, r):
    r₁ ← HighBits(r)
    v₁ ← HighBits(r + z)
    si r₁ ≠ v₁: devolver 1
    si no:       devolver 0
```

El hint es un simple bit booleano por coeficiente. Para un vector de $k$ polinomios, el vector $\mathbf{h}$ contiene $k \times 256$ bits. Sin embargo, FIPS 204 impone que el número total de bits a 1 en $\mathbf{h}$ no exceda $\omega$ (80, 55, o 75 según el nivel). Si esta cota se viola, la firma se rechaza y se reinicia el proceso (*abort*).

```
Estructura del vector hint h (para ML-DSA-44, k=4):
                                                          
  h.vec[0]:  [0 0 0 1 0 0 ... 0 0 1 0]   256 coeficientes
  h.vec[1]:  [0 0 0 0 0 0 ... 0 0 0 0]   (cada uno 0 o 1)
  h.vec[2]:  [0 0 1 0 0 0 ... 0 0 0 0]
  h.vec[3]:  [0 0 0 0 0 1 ... 0 0 0 0]
             ─────────────────────────
             Total de 1s ≤ ω = 80
```

**Implementación:**

```c
int32_t make_hint(int32_t z, int32_t r) {
    int32_t r1, v1;
    r1 = highbits(r);
    v1 = highbits(r + z);
    return (r1 != v1);
}
```

Y la extensión a polinomios, que además cuenta los hints activos:

```c
unsigned int poly_make_hint(poly *h, const poly *z, const poly *r) {
    unsigned int i, count = 0;
    for (i = 0; i < N; ++i) {
        h->coeffs[i] = make_hint(z->coeffs[i], r->coeffs[i]);
        count += (unsigned int)h->coeffs[i];
    }
    return count;
}
```

La firma invoca `poly_make_hint` sobre cada componente del vector y acumula el conteo total. Si `count_total > OMEGA`, se aborta y se reintenta con un nuevo $\mathbf{y}$.

---

### 21.3 `UseHint`: Reconstrucción asistida

El verificador recibe el hint $h$ y un valor $r$ (calculado a partir de la firma y la clave pública). Debe reconstruir los bits altos correctos ajustando el resultado de `HighBits(r)` cuando el hint indica un carry.

**Definición (FIPS 204, Algoritmo 40):**

```
UseHint(h, r):
    (r₁, r₀) ← Decompose(r)
    si h = 0:
        devolver r₁
    si r₀ > 0:
        devolver (r₁ + 1) mod m     donde m = (Q-1) / α
    si no:
        devolver (r₁ - 1) mod m
```

El valor $m = (Q-1) / (2\gamma_2)$ es el número total de regiones posibles para $r_1$ (44 para ML-DSA-44, 16 para ML-DSA-65/87). La aritmética modular en $r_1$ garantiza que el ajuste envuelva correctamente en los extremos (si $r_1 = m - 1$ y se incrementa, vuelve a 0; si $r_1 = 0$ y se decrementa, retorna a $m - 1$).

La dirección del ajuste ($+1$ o $-1$) depende del signo del residuo $r_0$:

- Si $r_0 > 0$: el valor $r$ estaba en la mitad superior de la región $r_1$, cerca del borde superior. El carry va *hacia arriba*: $r_1 \to r_1 + 1$.
- Si $r_0 \leq 0$: el valor $r$ estaba en la mitad inferior, cerca del borde inferior. El carry va *hacia abajo*: $r_1 \to r_1 - 1$.

**Implementación:**

```c
#define M ((Q - 1) / (2 * GAMMA2))

int32_t use_hint(int32_t h, int32_t r) {
    int32_t r1, r0;
    decompose(&r1, &r0, r);

    if (h == 0)
        return r1;

    if (r0 > 0)
        return (r1 + 1) % M;
    else
        return (r1 - 1 + M) % M;   /* +M para evitar resultado negativo */
}
```

Extensión polinómica:

```c
void poly_use_hint(poly *r1, const poly *h, const poly *r) {
    unsigned int i;
    for (i = 0; i < N; ++i)
        r1->coeffs[i] = use_hint(h->coeffs[i], r->coeffs[i]);
}
```

> **Nota sobre tiempo constante.** Al igual que `decompose`, las funciones `MakeHint` y `UseHint` operan sobre datos públicos (el compromiso, la firma, la clave pública). Las ramas condicionales son seguras aquí. La información secreta (la clave privada $\mathbf{s}_1$, $\mathbf{s}_2$, $\mathbf{t}_0$) no interviene en las entradas de estas funciones.

---

## 22. Contratos de Interfaz

### 22.1 Header `inc/poly.h`

Recopilación de todas las interfaces de la Fase 3:

```c
#ifndef POLY_H
#define POLY_H

#include <stdint.h>
#include "arithmetic.h"

#define N 256

/* === Selección de nivel de seguridad (compilación) === */
#ifndef DILITHIUM_MODE
#define DILITHIUM_MODE 2
#endif

#if DILITHIUM_MODE == 2
#define K 4
#define L 4
#define ETA 2
#define TAU 39
#define BETA 78
#define GAMMA1 (1 << 17)
#define GAMMA2 ((Q - 1) / 88)
#define OMEGA 80
#elif DILITHIUM_MODE == 3
#define K 6
#define L 5
#define ETA 4
#define TAU 49
#define BETA 196
#define GAMMA1 (1 << 19)
#define GAMMA2 ((Q - 1) / 32)
#define OMEGA 55
#elif DILITHIUM_MODE == 5
#define K 8
#define L 7
#define ETA 2
#define TAU 60
#define BETA 120
#define GAMMA1 (1 << 19)
#define GAMMA2 ((Q - 1) / 32)
#define OMEGA 75
#else
#error "DILITHIUM_MODE debe ser 2, 3, o 5"
#endif

#define D  13
#define M  ((Q - 1) / (2 * GAMMA2))

/* === Tipos === */
typedef struct { int32_t coeffs[N]; } poly;
typedef struct { poly vec[L]; }       polyvecl;
typedef struct { poly vec[K]; }       polyveck;

/* === Aritmética de polinomios === */
void poly_add(poly *r, const poly *a, const poly *b);
void poly_sub(poly *r, const poly *a, const poly *b);
void poly_reduce(poly *a);
void poly_caddq(poly *a);

/* === Descomposición y compresión === */
void power2round(int32_t *r1, int32_t *r0, int32_t a);
void decompose(int32_t *r1, int32_t *r0, int32_t a);
int32_t highbits(int32_t a);
int32_t lowbits(int32_t a);

void poly_power2round(poly *r1, poly *r0, const poly *a);
void poly_decompose(poly *r1, poly *r0, const poly *a);
void poly_highbits(poly *r1, const poly *a);
void poly_lowbits(poly *r0, const poly *a);

/* === Hints === */
int32_t make_hint(int32_t z, int32_t r);
int32_t use_hint(int32_t h, int32_t r);
unsigned int poly_make_hint(poly *h, const poly *z, const poly *r);
void poly_use_hint(poly *r1, const poly *h, const poly *r);

/* === Vectores de polinomios === */
void polyvecl_ntt(polyvecl *v);
void polyvecl_invntt(polyvecl *v);
void polyveck_ntt(polyveck *v);
void polyveck_invntt(polyveck *v);
void polyvecl_pointwise_acc(poly *r, const poly *a, const polyvecl *b);

void polyveck_add(polyveck *r, const polyveck *a, const polyveck *b);
void polyveck_sub(polyveck *r, const polyveck *a, const polyveck *b);
void polyveck_reduce(polyveck *v);
void polyveck_caddq(polyveck *v);
void polyveck_power2round(polyveck *r1, polyveck *r0, const polyveck *v);
void polyveck_decompose(polyveck *r1, polyveck *r0, const polyveck *v);
unsigned int polyveck_make_hint(polyveck *h, const polyveck *z, const polyveck *r);
void polyveck_use_hint(polyveck *r1, const polyveck *h, const polyveck *r);

#endif /* POLY_H */
```

---

### 22.2 Matriz de Pruebas Unitarias

La validación de la Fase 3 se estructura en cuatro bloques de pruebas que verifican las propiedades algebraicas de cada subsistema:

| # | Test | Propiedad verificada | Criterio de éxito |
|---|------|----------------------|-------------------|
| **T1** | `test_power2round` | Para cada $r \in \{0, 1, Q/2, Q-1\}$: $r_1 \cdot 2^{13} + r_0 \equiv r \pmod{Q}$ | Igualdad exacta |
| **T2** | `test_power2round_bounds` | Para 10 000 valores aleatorios: $r_1 \in [0, 1\,023]$ y $r_0 \in (-2^{12}, 2^{12}]$ | Todas las cotas satisfechas |
| **T3** | `test_decompose` | Para cada $r \in \{0, 1, \gamma_2, Q-1\}$: $r_1 \cdot 2\gamma_2 + r_0 \equiv r \pmod{Q}$ | Igualdad exacta |
| **T4** | `test_decompose_corner_case` | Para $r = Q - 1$: $r_1 = 0$ | Corner case correcto |
| **T5** | `test_decompose_bounds` | Para 10 000 valores aleatorios: $r_1 \in [0, M-1]$ y $|r_0| < \gamma_2$ | Todas las cotas satisfechas |
| **T6** | `test_highbits_lowbits` | `HighBits(r) * 2γ₂ + LowBits(r) ≡ r (mod Q)` | Igualdad exacta |
| **T7** | `test_hint_roundtrip` | Para valores aleatorios $r, z$ con $|z| < \gamma_2$: `UseHint(MakeHint(z, r), r+z)` = `HighBits(r+z)` si no hay carry, o el valor corregido si lo hay | Reconstrucción correcta |
| **T8** | `test_hint_count` | Para signaturas simuladas: total de hints activos $\leq \omega$ | Cota respetada |
| **T9** | `test_polyvec_ntt_roundtrip` | `polyveck_invntt(polyveck_ntt(v))` recupera $v$ (con pointwise por identidad intermedio) | Igualdad coeficiente a coeficiente |
| **T10** | `test_pointwise_acc_overflow` | Acumulación de $\ell$ productos pointwise no excede `INT32_MAX` | Sin desbordamiento |

---

## 23. Decisiones de Ingeniería — Fase 3

| Decisión | Alternativa Descartada | Razón |
|---|---|---|
| `struct poly` con array tipado | Array `int32_t[256]` desnudo | Tipado fuerte, paso por puntero seguro, alineación predecible |
| Parámetros por `#define` en compilación | Struct de parámetros en runtime | Sin overhead de indirección; el compilador optimiza constantes conocidas |
| Operador `%` en `decompose` | Barrett genérico para $\alpha$ | $\alpha$ no es potencia de 2; la función opera sobre datos públicos, la instrucción `SDIV` es tolerable |
| Ramas en `decompose`/`use_hint` | Aritmética branchless | Los datos de entrada son públicos (compromiso, firma), no secretos |
| `polyvecl_pointwise_acc` acumula sin reducción intermedia | `barrett_reduce` entre cada suma | $\ell \cdot Q < 2^{26} \ll 2^{31}$: no hay riesgo de overflow antes de la reducción final |
| Matriz $\mathbf{A}$ generada fila a fila | Materializar $\mathbf{A}$ completa en RAM | Ahorro de $(k-1) \times \ell$ KiB de RAM (16–56 KiB según nivel de seguridad) |

---

## 24. Hoja de Ruta — Fases Futuras

- ~~**Fase 1:** Aritmética modular (Montgomery, Barrett, reducciones condicionales).~~ ✅
- ~~**Fase 2:** NTT de longitud 256 sobre `Z_Q`.~~ ✅
- **Fase 3:** Polinomios, vectores y algoritmos de compresión. *(en curso)*
- **Fase 4:** Funciones de hash (SHAKE-128/256) para la generación de matrices y masking.
- **Fase 5:** Keygen, Sign y Verify completos según FIPS 204.

---

*Documento de arquitectura — Fases 1, 2 y 3 | `el_pedestal` | ML-DSA bare-metal en C99 de 32 bits*
