# DESIGN.md — Diseño Arquitectónico: Fase 1 de `el_pedestal`

> **Alcance:** Este documento describe la arquitectura de software, las decisiones de ingeniería y los contratos de interfaz de las primitivas aritméticas de Fase 1. Las demostraciones matemáticas rigurosas (Bézout, Hensel, derivación de constantes, pruebas de cota) residen en [`docs/MATH_PROOFS.md`](docs/MATH_PROOFS.md).

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

$$f = N^{-1} \\cdot R^2 \\pmod{Q} = 8347681 \\cdot 2365951 \\pmod{8380417} = 41978$$

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

## 17. Hoja de Ruta — Fases Futuras

- ~~**Fase 1:** Aritmética modular (Montgomery, Barrett, reducciones condicionales).~~ ✅
- ~~**Fase 2:** NTT de longitud 256 sobre `Z_Q`.~~ ✅
- **Fase 3:** Operaciones de polinomios (`polyvec_*`) y operaciones de módulo.
- **Fase 4:** Funciones de hash (SHAKE-128/256) para la generación de matrices y masking.
- **Fase 5:** Keygen, Sign y Verify completos según FIPS 204.

---

*Documento de arquitectura — Fases 1 y 2 | `el_pedestal` | ML-DSA bare-metal en C99 de 32 bits*
