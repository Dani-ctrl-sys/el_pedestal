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

## 9. Hoja de Ruta — Fases Futuras

- **Fase 2:** NTT (Number Theoretic Transform) de longitud 256 sobre `Z_Q`, usando raíces de unidad de orden 256.
- **Fase 3:** Operaciones de polinomios (`polyvec_*`) y operaciones de módulo.
- **Fase 4:** Funciones de hash (SHAKE-128/256) para la generación de matrices y masking.
- **Fase 5:** Keygen, Sign y Verify completos según FIPS 204.

---

*Documento de arquitectura — Fase 1 | `el_pedestal` | ML-DSA bare-metal en C99 de 32 bits*
