# STUDY_GUIDE.md — Guía de Estudio Completa: Fases 1 y 2 de `el_pedestal`

> **Propósito:** Este documento es tu recurso personal de estudio. Explica **todo** lo que ocurre en las Fases 1 y 2 del proyecto: qué hace cada pieza, **por qué** la necesitamos, y cómo encajan entre sí. Está pensado para que lo leas, lo releas y te sirva de referencia cuando necesites recordar cualquier concepto.
>
> - Para los contratos de interfaz y decisiones de ingeniería → [`DESIGN.md`](../DESIGN.md)
> - Para las demostraciones algebraicas formales → [`MATH_PROOFS.md`](MATH_PROOFS.md)

---

## Índice

1. [El Panorama General: ¿Qué estamos construyendo y por qué?](#1-el-panorama-general)
2. [El Cuerpo Finito Z_Q: Nuestro universo aritmético](#2-el-cuerpo-finito-z_q)
3. [El Problema del Desbordamiento: Por qué no podemos simplemente usar `%`](#3-el-problema-del-desbordamiento)
4. [Reducción de Montgomery: Dividir sin dividir](#4-reducción-de-montgomery)
5. [Reducción de Barrett: Aproximar el cociente con shifts](#5-reducción-de-barrett)
6. [Reducciones Condicionales: Los guardianes del rango](#6-reducciones-condicionales)
7. [Seguridad de Tiempo Constante: Por qué prohibimos los `if`](#7-seguridad-de-tiempo-constante)
8. [Mapa de Constantes: El diccionario rápido](#8-mapa-de-constantes)
9. [Cómo encaja todo: El flujo completo de un dato](#9-cómo-encaja-todo)

**Fase 2 — NTT (Number Theoretic Transform):**

10. [¿Qué es la NTT y por qué la necesitamos?](#10-qué-es-la-ntt-y-por-qué-la-necesitamos)
11. [Las raíces de la unidad: el corazón de la NTT](#11-las-raíces-de-la-unidad-el-corazón-de-la-ntt)
12. [La tabla de zetas: Bit-Reversal y dominio Montgomery](#12-la-tabla-de-zetas-bit-reversal-y-dominio-montgomery)
13. [La operación mariposa (butterfly)](#13-la-operación-mariposa-butterfly)
14. [La NTT capa por capa](#14-la-ntt-capa-por-capa)
15. [La NTT inversa (INTT)](#15-la-ntt-inversa-intt)
16. [El factor de normalización f = 41978](#16-el-factor-de-normalización-f--41978)
17. [Multiplicación Pointwise](#17-multiplicación-pointwise)
18. [El flujo completo de la multiplicación de polinomios](#18-el-flujo-completo-de-la-multiplicación-de-polinomios)
19. [Mapa de Constantes — Fase 2](#19-mapa-de-constantes--fase-2)
20. [Verificación de integridad: el test NTT → INTT](#20-verificación-de-integridad-el-test-ntt--intt)

---

## 1. El Panorama General

### ¿Qué es `el_pedestal`?

`el_pedestal` es una implementación en C99 del algoritmo de firma digital **ML-DSA** (anteriormente conocido como CRYSTALS-Dilithium), estandarizado en **FIPS 204** por el NIST. Es un algoritmo de criptografía **post-cuántica**, lo que significa que está diseñado para resistir ataques de computadores cuánticos.

### ¿Por qué "el pedestal"?

Porque la Fase 1 es literalmente el **pedestal** sobre el que se sostiene todo lo demás. Sin una aritmética modular correcta, eficiente y segura, nada de lo que construyamos encima (NTT, polinomios, firmas) funcionará.

### ¿Qué hace la Fase 1 exactamente?

La Fase 1 implementa las **cuatro operaciones aritméticas fundamentales** que necesita ML-DSA para trabajar dentro del cuerpo finito `Z_Q`:

| Función              | Rol                                                        |
|----------------------|------------------------------------------------------------|
| `montgomery_reduce`  | Reduce el resultado de una multiplicación modular          |
| `barrett_reduce`     | Reduce un coeficiente individual al rango centrado         |
| `conditional_subq`   | Normaliza un valor al rango canónico `[0, Q)`             |
| `caddq`              | Ajusta un valor negativo sumándole `Q` si es necesario     |

Estas cuatro funciones son los **cimientos**. Todo lo que venga después (la NTT en Fase 2, los polinomios en Fase 3, las firmas en Fase 5) las invocará miles de veces.

### ¿Por qué no usar simplemente `a % Q`?

Tres razones fundamentales:

1. **Rendimiento:** La operación de módulo (`%`) en hardware se traduce en una instrucción de **división entera**, que es enormemente lenta en procesadores embebidos de 32 bits (puede costar 20-40 ciclos frente a 1-3 ciclos de una suma o shift).
2. **Seguridad:** La instrucción de división en muchos procesadores tarda un tiempo variable dependiendo del valor de los operandos. Esto filtra información secreta por un **canal lateral de tiempo**.
3. **Desbordamiento:** Cuando multiplicamos dos coeficientes de 23 bits, el resultado puede tener hasta 46 bits. No cabe en un `int32_t` y necesitamos técnicas especiales para "traerlo de vuelta" a 32 bits.

Por estas tres razones, reemplazamos **toda** la aritmética modular por técnicas especializadas: Montgomery y Barrett.

---

## 2. El Cuerpo Finito Z_Q

### ¿Qué es un cuerpo finito?

Un **cuerpo finito** (o campo finito) es un conjunto finito de números donde puedes sumar, restar, multiplicar y dividir (excepto por cero) y siempre obtienes un resultado que pertenece al mismo conjunto. Es un "universo cerrado" de números.

### ¿Por qué Q = 8 380 417?

Este número no fue elegido al azar. Es el primo que define el estándar ML-DSA (FIPS 204), y tiene propiedades excepcionales:

1. **Es primo.** Esto garantiza que `Z_Q` sea un cuerpo (no solo un anillo), lo que significa que todo elemento no nulo tiene inverso multiplicativo. Puedes "dividir" sin problemas.

2. **Q ≡ 1 (mod 256).** Esta propiedad es **crucial** para la Fase 2 (NTT). Significa que existen raíces primitivas de la unidad de orden 256 dentro de `Z_Q`. Sin esta propiedad, la NTT de longitud 256 que necesita ML-DSA sería imposible sobre este cuerpo.

   > **¿Qué es una raíz de la unidad?** Una raíz *n*-ésima de la unidad en `Z_Q` es un número `w` tal que `w^n ≡ 1 (mod Q)` pero `w^k ≢ 1 (mod Q)` para todo `0 < k < n`. Es el equivalente modular de los números complejos `e^(2πi/n)` que se usan en la FFT clásica. La propiedad `Q ≡ 1 (mod 256)` garantiza, por un resultado de la teoría de grupos (el grupo multiplicativo `Z_Q*` es cíclico de orden `Q-1`), que existe un elemento de orden exactamente 256. Ese elemento es la "raíz zeta" que usaremos en la NTT de Fase 2.

3. **Q cabe en 23 bits.** Dado que `Q = 8 380 417 < 2^23 = 8 388 608`, un coeficiente reducido cabe holgadamente en un `int32_t` de 32 bits. Esto nos deja **9 bits de margen** antes de desbordar, lo que permite acumular varias sumas sin tener que reducir después de cada operación.

4. **Q es impar.** Esto es necesario para que `Q` sea invertible módulo `2^32` (requisito de Montgomery). Si `Q` fuese par, compartiría un factor con `2^32` y no existiría el inverso.

### La aritmética en Z_Q

Toda operación en ML-DSA se realiza **módulo Q**. Esto significa que después de sumar, restar o multiplicar dos números, tomamos el resto de dividir por `Q`:

```
Ejemplo: 8 000 000 + 1 000 000 = 9 000 000
         9 000 000 mod 8 380 417 = 619 583
```

El resultado "se envuelve" (wrap around) y vuelve al rango válido. Es como un reloj, pero en lugar de dar la vuelta en 12 o 24, da la vuelta en 8 380 417.

### Representación centrada vs. no centrada

Hay dos formas de representar los elementos de `Z_Q`:

- **Rango canónico (no centrado):** `[0, Q)` → valores de `0` a `8 380 416`.
- **Rango centrado:** `[-Q/2, Q/2)` → valores de `-4 190 208` a `4 190 208`.

¿Por qué nos importa? Porque ML-DSA necesita **ambas** representaciones en distintas partes del algoritmo:

- La **reducción de Barrett** produce resultados en el rango centrado `[-Q/2, Q/2]`. Esto es útil porque permite hacer **varias operaciones seguidas** (sumas, restas) sin reducir entre medias, ya que los valores centrados son más pequeños en valor absoluto.
- La función `conditional_subq` produce resultados en el rango canónico `[0, Q)`. Esto es necesario al final, cuando hay que **serializar** los coeficientes para enviarlos por la red o almacenarlos.

---

## 3. El Problema del Desbordamiento

### ¿Por qué es un problema multiplicar?

Consideremos dos coeficientes `a` y `b`, ambos en el rango `[0, Q)`:

```
a_max = Q - 1 = 8 380 416    (23 bits)
b_max = Q - 1 = 8 380 416    (23 bits)
a_max * b_max = 70 231 374 530 304    (47 bits)
```

El producto de dos coeficientes de 23 bits necesita **hasta 47 bits**. Un `int32_t` solo tiene 32 bits (31 bits + signo). No cabe. Necesitamos un `int64_t` para almacenar el producto intermedio, y después debemos **reducirlo** de vuelta a 32 bits.

### Las dos estrategias de reducción

Para traer ese número grande de vuelta al rango de `Z_Q`, tenemos dos técnicas distintas, cada una optimizada para un caso de uso diferente:

| Técnica       | Entrada típica  | Salida             | Cuándo se usa                          |
|---------------|----------------|--------------------|----------------------------------------|
| **Montgomery** | `int64_t` (producto de 64 bits) | `int32_t` en `[-Q, Q]`  | Después de multiplicar dos coeficientes |
| **Barrett**    | `int32_t` (suma/acumulación)    | `int32_t` en `[-Q/2, Q/2]` | Después de sumar/acumular coeficientes  |

**¿Por qué dos técnicas y no una sola?**

- **Montgomery** es ideal para multiplicaciones porque acepta entradas de 64 bits. Pero tiene un "precio": opera en un "dominio especial" (el dominio de Montgomery) que añade un factor de escala `2^32`. Esto es irrelevante cuando haces muchas multiplicaciones seguidas (el factor se cancela), pero sería un estorbo para operaciones simples.
- **Barrett** es ideal para reducciones simples de 32 bits (tras sumas o restas acumuladas). Es directa: la entrada y la salida están en el "dominio normal" sin factores de escala extraños.

---

## 4. Reducción de Montgomery

### La idea central: dividir por 2^32 en lugar de por Q

La división entera por `Q` es cara. Pero la división por `2^32` es **gratis**: es un simple desplazamiento a la derecha de 32 bits (`>> 32`). La genialidad de Montgomery es transformar el problema de "dividir por Q" en "dividir por 2^32".

### El Dominio de Montgomery

Para que este truco funcione, los coeficientes deben estar en una representación especial llamada **Dominio de Montgomery**. Un coeficiente `a` en el dominio normal se convierte al dominio de Montgomery como:

```
a_mont = a * 2^32  (mod Q)
```

¿Por qué hacemos esto? Porque cuando multiplicamos dos valores en el dominio de Montgomery y luego aplicamos la reducción de Montgomery, obtenemos el resultado correcto:

```
a_mont * b_mont = (a * 2^32) * (b * 2^32) = a * b * 2^64

Reducción de Montgomery (dividir por 2^32):
    a * b * 2^64 / 2^32 = a * b * 2^32 = (a*b)_mont
```

El resultado sigue en el dominio de Montgomery. Perfecto: podemos encadenar multiplicaciones sin salir nunca del dominio.

### El algoritmo paso a paso

Archivo: [`src/arithmetic.c`](../src/arithmetic.c)

```c
int32_t montgomery_reduce(int64_t a) {
    int32_t t   = (int32_t)a * QINV;                        // Paso 1
    int32_t res = (int32_t)((a - (int64_t)t * Q) >> 32);    // Paso 2
    return res;
}
```

Vamos a desmenuzar cada paso y entender **por qué** hace lo que hace:

#### Paso 1: Calcular el factor corrector `t`

```c
int32_t t = (int32_t)a * QINV;
```

**¿Qué ocurre aquí?**
- `(int32_t)a` toma solo los **32 bits inferiores** de `a` (truncamiento). Llamémoslo `a_low`.
- Se multiplica `a_low` por `QINV` (el inverso de `Q` módulo `2^32`).
- El resultado se trunca de nuevo a 32 bits. Esto implica que todo el cálculo ocurre **módulo 2^32** de forma implícita (gracias al desbordamiento natural de los enteros de 32 bits en complemento a dos).

**¿Por qué hacemos esto?**
El objetivo de `t` es ser el número mágico que, al multiplicarse por `Q`, "anula" exactamente los 32 bits inferiores de `a`. Es decir, queremos que `a - t*Q` tenga sus 32 bits inferiores todos a cero, para que la posterior división por `2^32` sea exacta (sin pérdida de información).

**¿Por qué funciona?**
Porque `QINV` es el inverso de `Q` módulo `2^32`. Esto significa que `Q * QINV ≡ 1 (mod 2^32)`. Entonces:

```
t * Q ≡ a_low * QINV * Q ≡ a_low * 1 ≡ a_low  (mod 2^32)
```

Por tanto:

```
a - t*Q ≡ a - a_low ≡ 0  (mod 2^32)
```

Los 32 bits inferiores de `(a - t*Q)` son **siempre cero**. Esto no es una coincidencia: es la consecuencia algebraica directa de la definición de `QINV`.

> **Demostración formal:** [`MATH_PROOFS.md`, Demostración 3](MATH_PROOFS.md#demostración-3-anulación-de-los-32-bits-inferiores-en-la-reducción-de-montgomery)

#### Paso 2: Desplazamiento exacto

```c
int32_t res = (int32_t)((a - (int64_t)t * Q) >> 32);
```

**¿Qué ocurre aquí?**
- Se calcula `a - t*Q` en aritmética de 64 bits (para no perder los bits superiores).
- Se desplaza el resultado 32 bits a la derecha. Esto es equivalente a dividir por `2^32`.
- El resultado se trunca a `int32_t`.

**¿Por qué podemos desplazar 32 bits sin perder información?**
Porque acabamos de demostrar que los 32 bits inferiores son todos cero. Desplazar a la derecha 32 posiciones descarta esos ceros y nos deja con un entero de 32 bits limpio.

**¿Qué obtenemos al final?**
El resultado `res` satisface:

```
res ≡ a * 2^(-32)  (mod Q)
```

Es decir, hemos "dividido" `a` por `2^32` en el sentido modular. Si `a` era un producto `(x_mont * y_mont)`, el resultado es `(x*y)_mont`: el producto en el dominio de Montgomery.

### La constante QINV = 58 728 449

Este es el inverso multiplicativo de `Q` módulo `2^32`. Cumple la propiedad fundamental:

```
Q * QINV = 8 380 417 * 58 728 449 = 492 168 892 383 233
492 168 892 383 233  mod  4 294 967 296  =  1
```

**¿Por qué existe este inverso?**
Porque `Q` es impar (es primo y distinto de 2). Todo número impar es invertible módulo una potencia de 2 (ya que `gcd(Q, 2^32) = 1`). Esta es la identidad de Bézout aplicada.

**¿Cómo se calcula?**
Mediante el **Lema de Hensel** (levantamiento 2-ádico): se empieza con una solución módulo 2 (`x₀ = 1`, trivial porque `Q` es impar) y se va "duplicando la precisión" en cada iteración: `mod 2 → mod 4 → mod 16 → mod 256 → ...` hasta llegar a `mod 2^32`.

> **Demostración formal:** [`MATH_PROOFS.md`, Demostraciones 1 y 2](MATH_PROOFS.md#demostración-1-identidad-de-bézout-y-existencia-del-inverso-de-q)

### ¿Cuándo usamos Montgomery?

En `el_pedestal`, Montgomery se usa **exclusivamente dentro de la NTT** (Fase 2). La NTT consiste en cientos de multiplicaciones encadenadas (multiplicar coeficientes por las raíces de la unidad "zetas"). Al convertir las zetas al dominio de Montgomery una sola vez (al inicio), todas las multiplicaciones posteriores se benefician del shift barato.

### El ciclo de vida del dominio de Montgomery

Es fundamental entender que el dominio de Montgomery no es "gratis": tiene un coste de entrada y salida. El truco es que ese coste se paga pocas veces comparado con las muchas multiplicaciones que se ahorran.

1. **Entrada al dominio:** Las constantes (las zetas de la NTT) se precomputan en el dominio de Montgomery **una sola vez**, fuera de línea, multiplicándolas por `2^32 mod Q`. Esto se hace en el script `tools/generate_zetas.py`.
2. **Operaciones dentro del dominio:** Cada multiplicación `coef * zeta_mont` seguida de `montgomery_reduce` produce el resultado correcto dentro del dominio. No hay coste extra de conversión.
3. **Salida del dominio:** Al final de la NTT inversa, se multiplica cada coeficiente por `f = mont^{-1} mod Q` (el factor de normalización, que incorpora tanto la escala `1/256` de la INTT como la des-conversión de Montgomery), seguido de una `montgomery_reduce`. Esto devuelve los coeficientes al dominio normal.

El ahorro neto es enorme: **256 conversiones de entrada (precomputadas off-line) + 256 conversiones de salida** frente a **256 × 7 = 1792 multiplicaciones** que se benefician del shift barato de Montgomery.

### Ejemplo numérico completo: Montgomery de principio a fin

Vamos a caminar por un cálculo real con números concretos para anclar la intuición.

**Datos de entrada:**
```
coef1 = 8 000 000    (un coeficiente dentro de Z_Q)
coef2 = 8 000 000
Producto de 64 bits: 8 000 000 × 8 000 000 = 64 000 000 000 000
```

Este producto tiene 46 bits. No cabe en 32 bits. Aplicamos `montgomery_reduce`:

**Paso 1 — Factor corrector:**
```
a = 64 000 000 000 000
a_low = a mod 2^32 = 64 000 000 000 000 mod 4 294 967 296

64 000 000 000 000 / 4 294 967 296 ≈ 14 901.16...
14 901 × 4 294 967 296 = 63 997 384 022 496
a_low = 64 000 000 000 000 - 63 997 384 022 496 = 2 615 977 504

t = a_low * QINV  (mod 2^32)
  = 2 615 977 504 * 58 728 449  (mod 2^32)
```

El valor exacto de `t` se obtiene truncando la multiplicación de 32×32 bits a 32 bits (desbordamiento natural en C).

**Paso 2 — Absorción y desplazamiento:**
```
resultado_64 = a - t * Q    (en 64 bits)
```

Sabemos que los 32 bits inferiores de `resultado_64` son todos cero (por la propiedad de QINV). Entonces:

```
res = resultado_64 >> 32    (desplazamiento aritmético)
```

**Verificación con el test del proyecto:**

En [`tests/test_arith.c:37-40`](../tests/test_arith.c), el test hace exactamente esto:
```c
int64_t producto_gigante = (int64_t)8000000 * 8000000;  // = 64 000 000 000 000
int32_t r5 = montgomery_reduce(producto_gigante);
```

El resultado `r5` satisface: `r5 ≡ 64 000 000 000 000 × 2^(-32) (mod Q)`. Esto significa que `r5` es la representación en el dominio de Montgomery del producto `8 000 000 × 8 000 000 mod Q`, escalado por `2^(-32)`.

---

## 5. Reducción de Barrett

### La idea central: estimar el cociente por punto fijo

Barrett resuelve un problema distinto a Montgomery. No necesita un "dominio especial". La entrada y la salida están en el dominio normal de `Z_Q`.

El objetivo es calcular `a mod Q` sin usar la instrucción de división. La estrategia es:

```
a mod Q = a - floor(a / Q) * Q
```

Si pudiéramos calcular `floor(a / Q)` de forma barata, tendríamos el módulo. Barrett propone **aproximar** `1/Q` con aritmética de punto fijo.

### La aproximación de punto fijo

La idea es reemplazar la división por `Q` con una multiplicación y un desplazamiento:

```
floor(a / Q) ≈ floor(a * m / 2^k)
```

donde `m ≈ 2^k / Q` es un entero precalculado. La multiplicación por `m` seguida del desplazamiento a la derecha por `k` bits simula la división por `Q`.

### ¿Por qué k = 26 y m = 8?

Necesitamos que `m = 2^k / Q` sea un número "bonito" (idealmente una potencia de 2, para que la multiplicación sea un simple shift). Probamos valores de `k`:

```
k = 23:  m = 2^23 / Q = 8 388 608 / 8 380 417 = 1.000...     → m = 1 (poco preciso)
k = 24:  m = 2^24 / Q = 16 777 216 / 8 380 417 = 2.001...     → m = 2
k = 25:  m = 2^25 / Q = 33 554 432 / 8 380 417 = 4.003...     → m = 4
k = 26:  m = 2^26 / Q = 67 108 864 / 8 380 417 = 8.007...     → m = 8  ← ¡PERFECTO!
```

Con `k = 26`, obtenemos `m = 8 = 2^3`. Esto es **extraordinariamente afortunado**: multiplicar por 8 es simplemente un desplazamiento a la izquierda de 3 bits (`a << 3`), lo cual cuesta prácticamente nada en hardware.

Además, el error de la aproximación es minúsculo:

```
2^26 / Q = 8.00703...
Error = 8.00703 - 8 = 0.00703 (menos del 0.1%)
```

> **Demostración formal:** [`MATH_PROOFS.md`, Demostración 4](MATH_PROOFS.md#demostración-4-derivación-de-m--8-para-la-reducción-de-barrett-con-k--26)

### El algoritmo paso a paso

Archivo: [`src/arithmetic.c`](../src/arithmetic.c)

```c
int32_t barrett_reduce(int32_t a) {
    int32_t t   = (int32_t)(((int64_t)a * BARRETT_MULTIPLIER + (1 << 25)) >> 26);
    int32_t res = a - t * Q;
    return res;
}
```

#### Paso 1: Estimar el cociente con redondeo

```c
int32_t t = (int32_t)(((int64_t)a * BARRETT_MULTIPLIER + (1 << 25)) >> 26);
```

Esto calcula:

```
t = round(a * 8 / 2^26) = round(a / Q * (8Q / 2^26))
```

**¿Qué hace cada parte?**

- `(int64_t)a * BARRETT_MULTIPLIER`: Multiplicar `a` por 8 (en 64 bits para evitar desbordamiento). Equivale a `a << 3`.
- `+ (1 << 25)`: **Inyectar 2^25 antes del desplazamiento**. Este es el truco del medio-bit que convierte un truncamiento (`floor`) en un **redondeo al entero más cercano** (`round`).
- `>> 26`: Dividir por `2^26`, completando la aproximación de punto fijo.

**¿Por qué inyectar 2^25?**

Sin la inyección, el desplazamiento `>> 26` haría un simple truncamiento:

```
t_trunc = floor(a * 8 / 2^26)
```

Con la inyección de `2^25 = 2^(k-1)`:

```
t_round = floor((a * 8 + 2^25) / 2^26) = round(a * 8 / 2^26)
```

Esto es equivalente a sumar 0.5 antes de truncar, que es la definición clásica de redondeo al entero más cercano. 

**¿Por qué nos importa redondear en vez de truncar?**

Porque el redondeo produce un residuo **centrado** en `[-Q/2, Q/2]`, mientras que el truncamiento produce un residuo en `[0, Q)`. El residuo centrado es esencial para ML-DSA por dos razones:

1. **Compatibilidad con la NTT:** La NTT de ML-DSA trabaja con coeficientes en representación con signo. Un rango centrado minimiza el valor absoluto de los coeficientes, lo que reduce el riesgo de desbordamiento en cálculos posteriores.
2. **Acumulación segura:** Si los coeficientes están en `[-Q/2, Q/2]` (unos 22 bits con signo), puedes sumar varios sin reducir entre medias, ya que tienes margen antes de llegar al límite de 31 bits de un `int32_t`.

> **Demostración formal:** [`MATH_PROOFS.md`, Demostración 6](MATH_PROOFS.md#demostración-6-inyección-de-225-para-redondeo-al-entero-más-cercano-y-confinamiento-en--q2-q2)

#### Paso 2: Corrección residual

```c
int32_t res = a - t * Q;
```

Simplemente resta `t` veces `Q` de la entrada original. Si la estimación del cociente `t` es perfecta, obtenemos exactamente `a mod Q`. Si `t` difiere del cociente exacto en ±1 (que es lo máximo que puede pasar, como demostramos), el residuo queda en el rango `[-Q/2, Q/2]`.

**¿Por qué el error del cociente es como máximo ±1?**

Porque el error total de la aproximación es menor que 1:

```
|error_punto_fijo| < 0.25    (por la calidad de la aproximación m = 8)
|error_redondeo|  ≤ 0.5      (por la propia naturaleza del redondeo)
|error_total|     < 0.75     (suma de ambos)
```

Dado que `t` es un entero, un error menor que 0.75 respecto al cociente real significa que `t` es el cociente exacto redondeado. Como máximo se desvía en 1 unidad.

> **Demostración formal:** [`MATH_PROOFS.md`, Demostración 5](MATH_PROOFS.md#demostración-5-cota-de-error-de-barrett-para-entradas-de-32-bits)

### ¿Cuándo usamos Barrett?

Barrett se usa para **reducir coeficientes individuales** después de sumas, restas o acumulaciones. Es la herramienta "de propósito general" para traer un valor de 32 bits al rango centrado, sin necesitar el dominio de Montgomery.

---

## 6. Reducciones Condicionales

Las reducciones condicionales son operaciones simples pero **críticas**. Son los "guardianes del rango" que se aplican como paso final para asegurar que un coeficiente esté exactamente donde lo necesitamos.

### `conditional_subq`: Normalización al rango canónico

Archivo: [`src/arithmetic.c`](../src/arithmetic.c)

```c
int32_t conditional_subq(int32_t a) {
    int32_t res  = a - Q;
    int32_t mask = res >> 31;
    return res + (Q & mask);
}
```

**¿Qué hace?** Dado un valor `a` en el rango `[0, 2Q)`, lo lleva al rango canónico `[0, Q)`.

**¿Por qué la necesitamos?** Después de ciertas operaciones (como la salida de Montgomery que puede devolver valores en `[0, 2Q)`), necesitamos garantizar que el coeficiente esté en `[0, Q)` para serializarlo o compararlo.

**¿Cómo funciona, paso a paso?**

1. **Resta tentativa:** Se calcula `res = a - Q`.
   - Si `a >= Q`: entonces `res >= 0`. La resta estaba justificada, `a` se sale del rango y hay que restar `Q`.
   - Si `a < Q`: entonces `res < 0`. La resta fue "excesiva", hay que deshacerla.

2. **Generar la máscara:** `mask = res >> 31`.
   - Si `res >= 0`: `mask = 0x00000000` (todos ceros).
   - Si `res < 0`: `mask = 0xFFFFFFFF` (todos unos).

3. **Compensar si fue excesiva:** `return res + (Q & mask)`.
   - Si `res >= 0`: `Q & 0x00000000 = 0`. Se devuelve `res = a - Q`. Correcto.
   - Si `res < 0`: `Q & 0xFFFFFFFF = Q`. Se devuelve `res + Q = (a - Q) + Q = a`. Correcto.

**Resultado:** La función devuelve `a - Q` si `a >= Q`, o `a` si `a < Q`. Todo sin ningún salto condicional.

### `caddq`: Ajuste de negativos

Archivo: [`src/arithmetic.c`](../src/arithmetic.c)

```c
int32_t caddq(int32_t a) {
    int32_t mask = a >> 31;
    return a + (Q & mask);
}
```

**¿Qué hace?** Dado un valor `a` en el rango `(-Q, Q)`, si es negativo le suma `Q` para llevarlo a `[0, Q)`.

**¿Por qué la necesitamos?** Es la operación complementaria de `conditional_subq`. Después de una resta entre dos coeficientes reducidos (`x - y` con `x, y ∈ [0, Q)`), el resultado puede ser negativo (hasta `-Q + 1`). `caddq` lo devuelve al rango positivo.

**¿Cómo funciona?**

1. **Generar la máscara directamente:** `mask = a >> 31`.
   - Si `a >= 0`: `mask = 0`. No se hace nada.
   - Si `a < 0`: `mask = 0xFFFFFFFF`. Se sumará `Q`.

2. **Aplicar la corrección:** `return a + (Q & mask)`.
   - Si `a >= 0`: devuelve `a` intacto.
   - Si `a < 0`: devuelve `a + Q`, que ahora está en `[0, Q)`.

---

## 7. Seguridad de Tiempo Constante

### ¿Por qué es esto tan importante?

En criptografía, los datos que manejamos son **secretos** (claves privadas, nonces, valores intermedios de la firma). Un atacante que observa el **tiempo** que tarda cada operación puede deducir información sobre esos secretos.

Esta clase de ataque se llama **ataque de canal lateral de tiempo** (timing side-channel attack) y es una amenaza real y demostrada en la práctica.

### ¿Qué hace que el código sea vulnerable?

Un salto condicional (`if`, `?:`, bucles con terminación variable) cuyo predicado depende de un valor secreto crea dos problemas:

1. **Predicción de saltos (Branch Prediction):** Las CPUs modernas tienen un predictor de saltos que "aprende" el patrón de las bifurcaciones. Si un `if (x < 0)` se ejecuta millones de veces con un valor secreto `x`, el predictor aprende el patrón de bits de `x`. Otro proceso malicioso puede consultar el estado del predictor y extraer esos bits.

2. **Caché de instrucciones (I-Cache):** Las dos ramas de un `if` ocupan diferentes líneas de la caché de instrucciones. Qué línea se carga (y cuál no) es observable por un atacante mediante técnicas como **Flush+Reload** o **Prime+Probe**.

### La solución: el multiplexor aritmético

En lugar de:

```c
// ¡VULNERABLE! El salto depende del valor secreto 'x'
if (x < 0) x += Q;
```

Escribimos:

```c
// SEGURO: las mismas 3 instrucciones se ejecutan siempre
int32_t mask = x >> 31;    // Extensión de signo
x += Q & mask;             // Suma condicional sin salto
```

**¿Por qué es esto seguro?**

Porque las instrucciones que ejecuta la CPU son **exactamente las mismas** independientemente del valor de `x`:

```asm
sar  eax, 31     ; Desplazamiento aritmético (SIEMPRE se ejecuta)
and  ebx, eax    ; AND de bits               (SIEMPRE se ejecuta)
add  eax, ebx    ; Suma                       (SIEMPRE se ejecuta)
```

No hay bifurcaciones. El predictor de saltos no ve nada. La caché de instrucciones carga siempre las mismas líneas. El tiempo de ejecución es **idéntico** para cualquier valor de `x`.

### La extensión de signo: `x >> 31`

Este es el truco fundamental que hace posible todo lo anterior. En un `int32_t` representado en complemento a dos:

- El bit más significativo (bit 31) es el **bit de signo**: `1` si el número es negativo, `0` si es no negativo.
- El desplazamiento aritmético a la derecha (`>>`) **propaga** ese bit de signo. Al desplazar 31 posiciones, todos los 32 bits del resultado se convierten en copias del bit de signo.

Resultado:
- Si `x >= 0`: `x >> 31 = 0x00000000` (32 ceros). **Máscara nula.**
- Si `x < 0`:  `x >> 31 = 0xFFFFFFFF` (32 unos).  **Máscara total.**

Esta máscara se puede usar con `AND` para "activar" o "desactivar" selectivamente un valor sin ningún salto.

### Nota importante sobre el estándar C99

El estándar C99 (§6.5.7) dice que el desplazamiento a la derecha de un entero con signo negativo es **implementation-defined** (definido por la implementación). Esto significa que el compilador no está *obligado* a propagar el bit de signo; podría, en teoría, rellenar con ceros.

**¿Por qué confiamos en que funciona?**

1. **Usamos `int32_t`, no `int`.** El tipo `int32_t` (de `<stdint.h>`) garantiza que la representación interna es **complemento a dos exacto** de 32 bits. Esto elimina la ambigüedad sobre la representación.
2. **Todas las arquitecturas objetivo lo implementan como desplazamiento aritmético.** En x86 (`sar`), ARM (`asr`) y RISC-V (`sra`), el desplazamiento a la derecha de un entero con signo **siempre** propaga el bit de signo. No existe ningún procesador moderno relevante donde no sea así.
3. **El estándar FIPS 204 asume este comportamiento.** La implementación de referencia de ML-DSA (publicada por el NIST) usa exactamente el mismo patrón `x >> 31` para sus reducciones condicionales.

En resumen: aunque el estándar C99 lo marca como "implementation-defined", en la práctica es universal y el ecosistema criptográfico lo da por garantizado en las plataformas objetivo.

> **Demostración formal:** [`MATH_PROOFS.md`, Demostración 7](MATH_PROOFS.md#demostración-7-extensión-de-signo-en-complemento-a-dos-y-aritmética-de-tiempo-constante)

### El patrón general: Select(condición, valor_si_true, valor_si_false)

El multiplexor aritmético es un patrón generalizable:

```c
// Forma general del multiplexor sin saltos:
int32_t mask = condicion >> 31;  // 0x00000000 o 0xFFFFFFFF
result = (val_false & ~mask) | (val_true & mask);
```

Y la forma simplificada cuando `val_false = 0` (que es la que más usamos):

```c
result = val_true & mask;
```

Esto es exactamente lo que hacen `caddq` y `conditional_subq`.

---

## 8. Mapa de Constantes

Referencia rápida de todas las constantes de la Fase 1 y su razón de ser:

| Constante       | Valor          | Tipo    | Por qué existe                                                                             |
|-----------------|----------------|---------|--------------------------------------------------------------------------------------------|
| `Q`             | `8 380 417`    | Primo   | Módulo del cuerpo `Z_Q`. Definido por FIPS 204. Primo NTT-friendly (Q ≡ 1 mod 256).       |
| `QINV`          | `58 728 449`   | Entero  | Inverso de `Q` módulo `2^32`. Necesario para la Fase 1 (factor corrector de Montgomery).   |
| `BARRETT_MULTIPLIER` | `8`       | `2^3`   | Aproximación de `2^26 / Q`. Permite sustituir la división por `Q` con un shift de 3 bits.  |
| `1 << 25`       | `33 554 432`   | `2^25`  | Constante de redondeo. Inyectada antes del shift `>> 26` para convertir truncamiento en redondeo. |
| `31`            | (shift)        | —       | Número de bits para extraer el signo de un `int32_t` en complemento a dos.                 |
| `32`            | (shift)        | —       | Número de bits que se anulan en Montgomery. Define el "factor de escala" `R = 2^32`.       |

### Relaciones entre constantes

```
Q * QINV ≡ 1  (mod 2^32)     ← Definición del inverso de Montgomery
m * Q = 8 * 8 380 417 = 67 043 336 ≈ 2^26 = 67 108 864    ← Aproximación de Barrett
2^26 - 8*Q = 65 528          ← Error absoluto de la aproximación (< Q)
```

---

## 9. Cómo Encaja Todo

### El flujo de vida de un coeficiente en ML-DSA

Para entender cómo se usan estas funciones juntas, veamos el recorrido típico de un coeficiente a lo largo del algoritmo:

```
                          DOMINIO NORMAL
                         ┌─────────────┐
                         │ Coeficiente │
                         │ original    │
                         │ 'a' ∈ Z_Q   │
                         └──────┬──────┘
                                │
                  ┌─────────────┴─────────────┐
                  ▼                           ▼
          [Suma / Resta]              [Multiplicación]
          Resultado en 32 bits        Resultado en 64 bits
                  │                           │
                  ▼                           ▼
          ┌───────────────┐          ┌────────────────┐
          │ barrett_reduce │         │montgomery_reduce│
          │ salida: [-Q/2, │         │ salida: [-Q, Q] │
          │         Q/2]   │         │                  │
          └───────┬───────┘          └────────┬────────┘
                  │                           │
                  ▼                           ▼
          ┌──────────────┐          ┌──────────────────┐
          │   caddq      │          │ conditional_subq  │
          │ si < 0: +Q   │          │ si >= Q: -Q       │
          │ salida: [0,Q) │         │ salida: [0, Q)    │
          └──────┬───────┘          └────────┬─────────┘
                  │                           │
                  └─────────────┬─────────────┘
                                ▼
                      ┌─────────────────┐
                      │  Coeficiente    │
                      │  normalizado    │
                      │  en [0, Q)      │
                      │  listo para     │
                      │  serializar     │
                      └─────────────────┘
```

### ¿Cuándo se usa cada función?

| Momento del algoritmo                              | Función usada                  | Por qué                                                |
|----------------------------------------------------|--------------------------------|--------------------------------------------------------|
| Después de multiplicar dos coeficientes (NTT)      | `montgomery_reduce`            | El producto es de 64 bits y estamos en dominio Montgomery |
| Después de acumular varias sumas/restas             | `barrett_reduce`               | El acumulador puede haber crecido, necesita reducirse    |
| Al final, antes de serializar la firma              | `conditional_subq` o `caddq`   | Para garantizar el rango canónico `[0, Q)`              |
| Después de una resta que puede dar negativo         | `caddq`                        | Para devolver el valor al rango positivo                |

### El principio de economía

Observa que **no reducimos después de cada operación**. Esto es intencional y tiene una justificación aritmética precisa:

**Cálculo del margen de bits disponible:**

```
Valor máximo tras Barrett:  |r| ≤ Q/2 = 4 190 208
Bits necesarios:            ceil(log2(4 190 208)) = 22 bits de magnitud
Bits disponibles en int32_t: 31 bits de magnitud (bit 31 es signo)
Margen:                     31 - 22 = 9 bits
```

Eso significa que podemos **acumular** (sumar) hasta `2^9 = 512` coeficientes reducidos por Barrett antes de que el acumulador desborde un `int32_t`:

```
512 × 4 190 208 = 2 145 386 496 < 2^31 = 2 147 483 648    ✅ (cabe)
513 × 4 190 208 = 2 149 576 704 > 2^31                      ❌ (desborda)
```

En la práctica, la NTT de ML-DSA hace como máximo **una suma o resta entre cada multiplicación** (la mariposa butterfly: `a + w*b` y `a - w*b`). Tras la multiplicación `w*b` aplicamos `montgomery_reduce`, que devuelve `|res| ≤ Q`. Luego sumamos/restamos con `a` (también `|a| ≤ Q`), obteniendo `|resultado| ≤ 2Q ≈ 2^24`. Muy lejos del límite de `2^31`.


Este diseño evita reducciones innecesarias y maximiza el rendimiento sin sacrificar la corrección.

---
---

# Fase 2: La NTT (Number Theoretic Transform)

---

## 10. ¿Qué es la NTT y por qué la necesitamos?

### El problema: multiplicar polinomios es caro

ML-DSA trabaja con **polinomios** de grado 255 (es decir, vectores de 256 coeficientes, cada uno en `Z_Q`). La operación más costosa del algoritmo es **multiplicar dos polinomios** entre sí.

Si multiplicamos dos polinomios de grado 255 de forma directa (el método "escolar"), cada coeficiente del resultado es la suma de hasta 256 productos. Hay 511 coeficientes en el resultado. La complejidad es `O(N²) = O(256²) = 65 536` multiplicaciones. Esto es **inaceptable** para un sistema embebido.

### La solución: transformar, multiplicar, destransformar

La NTT (Number Theoretic Transform) es el equivalente modular de la FFT (Fast Fourier Transform). La idea es:

```
Dominio de COEFICIENTES          Dominio de EVALUACIÓN (NTT)
                                 
  a(x) = a₀ + a₁x + ... + a₂₅₅x²⁵⁵        â = [â₀, â₁, ..., â₂₅₅]
  b(x) = b₀ + b₁x + ... + b₂₅₅x²⁵⁵        b̂ = [b̂₀, b̂₁, ..., b̂₂₅₅]
                                 
  Multiplicar: O(N²) = 65 536    Multiplicar: O(N) = 256
  (convolución)                  (componente a componente: ĉᵢ = âᵢ · b̂ᵢ)
```

El truco es que la NTT convierte una **convolución** (cara) en una **multiplicación componente a componente** (barata). El flujo completo es:

```
1. â = NTT(a)            ← O(N log N) = O(256 × 8) = 2048 operaciones
2. b̂ = NTT(b)            ← O(N log N)
3. ĉᵢ = âᵢ · b̂ᵢ          ← O(N) = 256 multiplicaciones
4. c = INTT(ĉ)           ← O(N log N)
```

**Coste total:** `3 × O(N log N) + O(N) ≈ 6 400` operaciones, frente a las `65 536` del método directo. **Ahorro de un factor 10×.**

### La analogía con la FFT

Si conoces la FFT (Fast Fourier Transform), la NTT es **exactamente lo mismo**, pero:

| FFT (señales)                          | NTT (criptografía)                        |
|----------------------------------------|--------------------------------------------|
| Números complejos `C`                  | Enteros modulares `Z_Q`                    |
| Raíz de la unidad: `e^(2πi/N)`        | Raíz de la unidad: `ζ = 1753` (mod Q)     |
| Multiplicación de punto flotante       | Multiplicación modular (Montgomery)        |
| Resultado aproximado (redondeo)        | Resultado **exacto** (aritmética entera)   |

La ventaja de la NTT sobre la FFT es que no hay errores de redondeo de punto flotante: todo es aritmética entera exacta.

---

## 11. Las raíces de la unidad: el corazón de la NTT

### ¿Qué es una raíz de la unidad?

En el contexto de `Z_Q`, una **raíz $n$-ésima de la unidad** es un número `w` tal que:

```
w^n ≡ 1  (mod Q)        ← "dar la vuelta completa" vuelve a 1
w^k ≢ 1  (mod Q)        para todo 0 < k < n    ← no "llega antes"
```

Es como un reloj modular: si avanzas `n` pasos de tamaño `w`, vuelves al punto de partida (el 1). Pero si avanzas menos de `n` pasos, no has completado la vuelta.

### ¿Por qué necesitamos orden 512 y no 256?

Esta es una sutileza **crucial** que diferencia la NTT de ML-DSA de una NTT genérica.

ML-DSA no trabaja en el anillo `Z_Q[X] / (X^256 - 1)`, sino en `Z_Q[X] / (X^256 + 1)`. La diferencia es enorme:

- Con `(X^256 - 1)`: necesitamos raíces de `X^256 = 1`, es decir, raíces de orden 256.
- Con `(X^256 + 1)`: necesitamos raíces de `X^256 = -1`, es decir, un `w` tal que `w^256 ≡ -1 (mod Q)`.

Si `w^256 = -1`, entonces `w^512 = (-1)^2 = 1`. Pero `w^256 ≠ 1`. Por tanto, `w` tiene orden exactamente **512**.

### ¿Por qué Z_Q[X] / (X^256 + 1) y no (X^256 - 1)?

Porque el anillo `Z_Q[X] / (X^256 + 1)` es el anillo de polinomios **ciclotómico** que ML-DSA necesita para la seguridad del esquema lattice-based.

> **¿Qué significa "ciclotómico"?** La palabra viene del griego *kyklos* (círculo) + *temnein* (cortar). Un **polinomio ciclotómico** `Φₙ(X)` es el polinomio mínimo cuyas raíces son las raíces primitivas *n*-ésimas de la unidad. En nuestro caso, `X^256 + 1 = Φ₅₁₂(X)`: sus raíces complejas son exactamente las raíces primitivas 512-ésimas de la unidad. El hecho de que `Φ₅₁₂(X)` sea irreducible sobre los enteros (`Z`) garantiza que el anillo `Z_Q[X] / (Φ₅₁₂(X))` tenga la estructura algebraica "rígida" necesaria para que los problemas de retículas (*lattice problems*) sobre los que se basa ML-DSA sean demostrablemente difíciles.

### La constante ζ = 1753

`ZETA = 1753` es la raíz primitiva 512-ésima de la unidad en `Z_Q` elegida por el estándar. Se verifica:

```
1753^512 mod 8 380 417 = 1          ← tiene orden que divide a 512
1753^256 mod 8 380 417 = 8 380 416  ← que es Q - 1 ≡ -1 (mod Q)
```

El hecho de que `ζ^256 = -1` (y no 1) confirma que el orden es exactamente 512.

**¿Por qué existe una raíz de orden 512?**

Porque `Q - 1 = 8 380 416 = 2^13 × 1023`. Dado que `512 = 2^9` divide a `Q - 1` (y `9 ≤ 13`), el grupo multiplicativo `Z_Q*` (que es cíclico de orden `Q - 1`) contiene elementos de orden exactamente 512.

> **Demostración formal:** [`MATH_PROOFS.md`, Demostración 8](MATH_PROOFS.md#demostración-8-existencia-y-orden-de-la-raíz-de-la-unidad-zeta--1753)

---

## 12. La tabla de zetas: Bit-Reversal y dominio Montgomery

### ¿Qué contiene la tabla `zetas[]`?

La tabla `zetas[256]` contiene las 256 potencias de `ζ` que la NTT necesita durante sus 8 capas de butterflies. Pero no están almacenadas en orden natural (`ζ^0, ζ^1, ζ^2, ...`). Tienen dos transformaciones aplicadas:

### Transformación 1: Permutación bit-reversal

**¿Qué es?** El índice `i` de la tabla almacena `ζ` elevado a la potencia `bitrev(i)`, donde `bitrev` invierte los 8 bits binarios del índice.

**Ejemplo:**
```
i = 1  →  binario: 00000001  →  invertido: 10000000  →  bitrev(1) = 128
i = 2  →  binario: 00000010  →  invertido: 01000000  →  bitrev(2) = 64
i = 3  →  binario: 00000011  →  invertido: 11000000  →  bitrev(3) = 192
```

Entonces:
```
zetas[1] = ζ^128 · R mod Q = 25 847
zetas[2] = ζ^64  · R mod Q = 5 771 523
zetas[3] = ζ^192 · R mod Q = 7 861 508
```

**¿Por qué hacemos esto?** Porque la NTT de Cooley-Tukey accede a las zetas en un orden específico determinado por la estructura de las capas. Si almacenamos las zetas en orden bit-reversal, los tres bucles anidados de la NTT pueden acceder a ellas con un simple `k++` (acceso secuencial), sin necesidad de calcular índices complejos en tiempo de ejecución.

Capa por capa:
```
Capa 0 (len=128, 1 bloque):    necesita ζ^128           → k=1
Capa 1 (len=64,  2 bloques):   necesita ζ^64, ζ^192     → k=2,3
Capa 2 (len=32,  4 bloques):   necesita ζ^32, ζ^160, ζ^96, ζ^224  → k=4,5,6,7
...y así sucesivamente, cada capa duplica el número de bloques.
```

> **Demostración formal:** [`MATH_PROOFS.md`, Demostración 9](MATH_PROOFS.md#demostración-9-corrección-de-la-permutación-bit-reversal-en-la-tabla-de-zetas)

### Transformación 2: Conversión al dominio de Montgomery

Cada zeta se almacena multiplicada por `R = 2^32 mod Q`:

```
zetas[i] = ζ^(bitrev(i)) · 2^32  mod Q
```

**¿Por qué?** Porque dentro de la NTT, cada butterfly hace:

```c
t = montgomery_reduce((int64_t)zeta * a[j + len]);
```

El `montgomery_reduce` divide por `R = 2^32`. Si la zeta ya tiene un factor `R` incorporado, la reducción lo cancela:

```
montgomery_reduce(zeta_mont * b) = (ζ·R · b) / R = ζ · b  (mod Q)
```

Resultado limpio, sin factores de Montgomery residuales. Las 256 conversiones se pagan **una sola vez** (off-line, en `generate_zetas.py`), y las 1792 multiplicaciones de la NTT se benefician.

---

## 13. La operación mariposa (butterfly)

### ¿Qué es una mariposa?

La **mariposa** (butterfly) es la operación atómica de la NTT. Toma dos elementos (`a[j]` y `a[j+len]`) y una raíz de torsión (`zeta`), y produce dos nuevos elementos:

```
Entrada:    a = a[j],    b = a[j+len],    ω = zeta

Salida:     a' = a + ω·b
            b' = a - ω·b
```

Es decir, calcula la suma y la diferencia del elemento original con el producto `ω·b`.

### ¿Por qué se llama "mariposa"?

Si dibujas las conexiones, forman una "X" que se asemeja a las alas de una mariposa:

```
a[j]     ──────●──────→  a[j]     = a + ω·b
               ╲╱
               ╱╲
a[j+len] ──●──────→  a[j+len] = a - ω·b
            ×ω
```

### El código de la butterfly directa (Cooley-Tukey)

```c
t = montgomery_reduce((int64_t)zeta * a[j + len]);   // t = ω · b  (mod Q)
a[j + len] = a[j] - t;                                // b' = a - t
a[j]       = a[j] + t;                                // a' = a + t
```

**¿Por qué funciona?**
1. Se calcula `t = ω · b mod Q` usando Montgomery (las zetas ya están en dominio Montgomery, así que el factor `R` se cancela).
2. Se actualiza `a[j+len]` **antes** de `a[j]`. Esto es importante: se usa el valor original de `a[j]` en ambas líneas.
3. El resultado es correcto módulo `Q`, y no se necesita reducción adicional porque los valores tienen suficiente margen (≤ `2Q`, que cabe en un `int32_t`).

### La butterfly inversa (Gentleman-Sande)

En la INTT, el orden de las operaciones se **invierte**: primero se suma/resta, y luego se multiplica.

```c
t = a[j];                                              // Guardar valor original
a[j]       = caddq(t + a[j + len]);                    // a' = a + b (normalizado)
a[j + len] = t - a[j + len];                           // diff = a - b
a[j + len] = montgomery_reduce((int64_t)zeta * a[j + len]);  // b' = (-ω) · diff
```

**Diferencias clave:**
- **Orden invertido:** primero suma/resta, luego multiplica (en vez de primero multiplicar y luego sumar/restar).
- **Zeta negada:** se usa `-zetas[k]` en lugar de `+zetas[k]`. Negar la raíz implementa la "conjugación" necesaria para invertir la transformación.
- **`caddq` en la suma:** Sin esto, la suma `t + a[j+len]` podría producir valores negativos que se acumularían capa tras capa, causando desbordamiento.

> **Demostración formal:** [`MATH_PROOFS.md`, Demostración 10](MATH_PROOFS.md#demostración-10-corrección-de-la-mariposa-de-cooley-tukey)

---

## 14. La NTT capa por capa

### La estructura de tres bucles anidados

La NTT directa tiene tres bucles que controlan la transformación:

```c
k = 1;
for (len = 128; len >= 1; len >>= 1) {           // Bucle 1: CAPAS
    for (start = 0; start < 256; start = j + len) { // Bucle 2: BLOQUES
        zeta = zetas[k++];                            // Una zeta por bloque
        for (j = start; j < start + len; ++j) {      // Bucle 3: BUTTERFLIES
            // ... operación mariposa ...
        }
    }
}
```

**Bucle 1 — Las capas (`len`):**
- Hay **8 capas** (porque `log2(256) = 8`).
- `len` empieza en 128 y se divide por 2 en cada capa: `128, 64, 32, 16, 8, 4, 2, 1`.
- `len` representa la "distancia" entre los dos elementos que participan en cada butterfly.

**Bucle 2 — Los bloques (`start`):**
- Cada capa tiene `256 / (2 × len)` bloques independientes.
- Cada bloque usa **una sola zeta** (la raíz de torsión para ese sub-problema).

**Bucle 3 — Las butterflies (`j`):**
- Dentro de cada bloque, hay `len` mariposas que procesan pares de elementos.

### Visualización de las primeras capas

```
Capa 0 (len=128): 1 bloque, 128 butterflies
  Zeta: ζ^128     Pares: (0,128), (1,129), (2,130), ..., (127,255)

Capa 1 (len=64):  2 bloques, 64 butterflies cada uno
  Bloque 0: ζ^64   Pares: (0,64), (1,65), ..., (63,127)
  Bloque 1: ζ^192  Pares: (128,192), (129,193), ..., (191,255)

Capa 2 (len=32):  4 bloques, 32 butterflies cada uno
  Bloque 0: ζ^32   Pares: (0,32), (1,33), ..., (31,63)
  Bloque 1: ζ^160  Pares: (64,96), (65,97), ..., (95,127)
  Bloque 2: ζ^96   Pares: (128,160), (129,161), ..., (159,191)
  Bloque 3: ζ^224  Pares: (192,224), (193,225), ..., (223,255)

  ... (capas 3-7 siguen el mismo patrón, duplicando bloques y dividiendo len)
```

### Conteo total de operaciones

```
Total de butterflies = 128 + 2×64 + 4×32 + 8×16 + 16×8 + 32×4 + 64×2 + 128×1
                     = 128 × 8 = 1024

Cada butterfly = 1 montgomery_reduce + 1 suma + 1 resta = 3 operaciones
Total = 1024 × 3 = 3072 operaciones aritméticas
```

Esto es un orden de magnitud menos que las `65 536` de la multiplicación directa.

### Ejemplo numérico concreto: NTT de 4 elementos

Para anclar la intuición, vamos a caminar por una NTT simplificada de **4 elementos** (N=4, 2 capas). Los principios son idénticos a la de 256 elementos, solo que más pequeños.

**Datos de entrada:** `a = [1, 2, 3, 4]`

**Capa 0 (len=2, 1 bloque, zeta = ζ^2):**
```
Butterfly(a[0], a[2], ζ²):  t = ζ² · a[2] = ζ² · 3
    a[0]' = a[0] + t = 1 + ζ²·3
    a[2]' = a[0] - t = 1 - ζ²·3

Butterfly(a[1], a[3], ζ²):  t = ζ² · a[3] = ζ² · 4
    a[1]' = a[1] + t = 2 + ζ²·4
    a[3]' = a[1] - t = 2 - ζ²·4
```

**Capa 1 (len=1, 2 bloques):**
```
Bloque 0 (zeta = ζ¹):
    Butterfly(a[0]', a[1]', ζ¹):  t = ζ¹ · a[1]'
        â[0] = a[0]' + t
        â[1] = a[0]' - t

Bloque 1 (zeta = ζ³):
    Butterfly(a[2]', a[3]', ζ³):  t = ζ³ · a[3]'
        â[2] = a[2]' + t
        â[3] = a[2]' - t
```

**Observa el patrón:**
- Capa 0: **un solo** zeta compartido entre todas las butterflies (distancia larga entre pares).
- Capa 1: **dos** zetas distintos, uno por bloque (distancia corta entre pares).
- Los zetas se leen en orden: `k=1 → ζ², k=2 → ζ¹, k=3 → ζ³` — exactamente el orden bit-reversal.

Este mismo patrón, escalado a 8 capas y 128 butterflies por capa, es lo que ocurre en la NTT de 256 elementos de `el_pedestal`.

---

## 15. La NTT inversa (INTT)

### ¿Qué hace la INTT?

La INTT es la operación inversa de la NTT. Toma los 256 valores evaluados (dominio NTT) y reconstruye los 256 coeficientes del polinomio original.

### Las diferencias respecto a la NTT directa

| Aspecto              | NTT directa (Cooley-Tukey)     | INTT (Gentleman-Sande)                  |
|----------------------|--------------------------------|------------------------------------------|
| `len` avanza         | `128 → 1` (dividiendo)        | `1 → 128` (multiplicando)               |
| `k` avanza           | `1 → 255` (ascendente)        | `255 → 1` (descendente)                 |
| Signo de zeta        | `+zetas[k]`                   | `-zetas[k]` (conjugado)                 |
| Butterfly            | Multiplica, luego suma/resta   | Suma/resta, luego multiplica            |
| `caddq`              | No                            | Sí (normalización intermedia)            |
| Normalización final  | No                            | Sí (multiplicar por `f = 41978`)         |

**¿Por qué invertir la dirección de `len` y `k`?**

La NTT directa "descompone" el polinomio de arriba hacia abajo (de bloques grandes a pequeños). La INTT lo "recompone" de abajo hacia arriba (de bloques pequeños a grandes). Es como desarmar y rearmar una estructura en orden inverso.

**¿Por qué negar la zeta?**

En la FFT clásica, la transformada inversa usa `e^(-2πi/N)` en lugar de `e^(+2πi/N)`. En la NTT, el equivalente es usar `-ζ^p` en lugar de `+ζ^p`. Negar la raíz es lo que "invierte la rotación" necesaria para deshacer la transformación.

**¿Por qué usar `caddq` en la butterfly inversa?**

En la INTT, la suma `t + a[j+len]` puede producir valores que crecen capa tras capa. Sin normalización, después de 8 capas los valores podrían desbordar un `int32_t`. La llamada a `caddq` mantiene los valores acotados al rango `[0, Q)` en cada iteración, previniendo el desbordamiento.

> **Demostración formal:** [`MATH_PROOFS.md`, Demostración 11](MATH_PROOFS.md#demostración-11-la-intt-es-la-inversa-exacta-de-la-ntt)

---

## 16. El factor de normalización f = 41978

### ¿Por qué necesitamos un factor final?

La NTT directa multiplica efectivamente por una matriz `W` de tamaño `256×256`. La INTT aplica la matriz inversa `W⁻¹ = (1/N) · W*` (donde `W*` es la conjugada). El factor `1/N = 1/256` es lo que "escala" la reconstrucción para que `INTT(NTT(a)) = a` (y no `256·a`).

### ¿Por qué f = 41978 y no simplemente 256⁻¹ mod Q?

Porque la multiplicación final se hace **a través de** `montgomery_reduce`:

```c
a[j] = montgomery_reduce((int64_t)a[j] * f);
```

Esto computa `a[j] · f · R⁻¹ mod Q` (donde `R = 2^32`). Queremos que el resultado sea `a[j] · N⁻¹ mod Q`. Pero hay que rastrear **exactamente** de dónde vienen los factores de `R`.

### Tracking preciso de los factores R (paso a paso)

Rastreemos el factor `R` que acompaña a un coeficiente en cada etapa del flujo `NTT → pointwise → INTT`:

```
Paso 1: poly_ntt(a)
  Cada butterfly: montgomery_reduce(zeta_mont · b) = (ζ·R · b) / R = ζ·b
  → Los R se cancelan. Tras la NTT, los coeficientes NO tienen factor R.
  → Estado del coeficiente: â[i]  (sin R)

Paso 2: poly_mul_pointwise(c, a, b)
  c[i] = montgomery_reduce(â[i] · b̂[i]) = â[i] · b̂[i] / R = â[i]·b̂[i] · R⁻¹
  → ¡Aquí aparece un R⁻¹!  ← Primera fuente de R⁻¹
  → Estado del coeficiente: ĉ[i] · R⁻¹

Paso 3: poly_invntt (las 8 capas de butterflies)
  Mismo análisis que la NTT: los R se cancelan en cada butterfly.
  → Estado del coeficiente: c[j] · R⁻¹  (el R⁻¹ del pointwise persiste)

Paso 4: Normalización final
  montgomery_reduce(c[j] · f) = c[j] · f · R⁻¹ = c[j] · R⁻¹ · f · R⁻¹
  → ¡Aquí aparece otro R⁻¹!  ← Segunda fuente de R⁻¹
  → Estado final: c[j] · f · R⁻²
```

Queremos que el estado final sea `c[j] · N⁻¹` (el coeficiente original dividido por 256). Por tanto:

```
f · R⁻² = N⁻¹
f = N⁻¹ · R²
```

El `R²` compensa exactamente los dos factores `R⁻¹` que se acumulan: uno del **pointwise** (paso 2) y otro del propio **montgomery_reduce final** (paso 4).

### El cálculo paso a paso

```
N⁻¹ mod Q = 256⁻¹ mod 8 380 417 = 8 347 681
R mod Q    = 2^32 mod 8 380 417  = 4 193 792
R² mod Q   = 4 193 792² mod Q   = 2 365 951

f = 8 347 681 × 2 365 951  mod  8 380 417  =  41 978
```

> **Demostración formal:** [`MATH_PROOFS.md`, Demostración 12](MATH_PROOFS.md#demostración-12-derivación-de-la-constante-de-normalización-f--41978)

---

## 17. Multiplicación Pointwise

### ¿Qué es?

Una vez que dos polinomios están en el dominio NTT (dominio de evaluación), multiplicarlos es **trivial**: simplemente multiplicamos componente a componente.

```c
void poly_mul_pointwise(int32_t c[256], const int32_t a[256], const int32_t b[256]) {
    for (int i = 0; i < 256; i++) {
        c[i] = montgomery_reduce((int64_t)a[i] * b[i]);
    }
}
```

### ¿Por qué funciona?

Por el **Teorema de la Convolución**: la NTT convierte la convolución (multiplicación de polinomios) en multiplicación componente a componente:

```
NTT(a · b) = NTT(a) ⊙ NTT(b)
```

donde `⊙` es la multiplicación elemento a elemento (pointwise).

Por tanto:

```
a · b = INTT(NTT(a) ⊙ NTT(b))
```

Este es el flujo completo de la multiplicación de polinomios en ML-DSA.

### ¿Cuánto cuesta?

Solo **256 multiplicaciones** + 256 `montgomery_reduce`. Comparado con las 65 536 multiplicaciones del método directo, esto es un ahorro brutal, y es la única razón por la que ML-DSA es viable en hardware embebido de 32 bits.

---

## 18. El flujo completo de la multiplicación de polinomios

```
      DOMINIO DE COEFICIENTES              DOMINIO NTT (EVALUACIÓN)
      
      a(x) = [a₀, a₁, ..., a₂₅₅]
                    │
                    ▼
              ┌──────────┐
              │ poly_ntt │  ← O(N log N) = 2048 operaciones
              └────┬─────┘
                   │
                   ▼
              â = [â₀, â₁, ..., â₂₅₅]      b̂ = [b̂₀, b̂₁, ..., b̂₂₅₅]
                          │                          │
                          └────────────┬─────────────┘
                                       ▼
                          ┌───────────────────────┐
                          │ poly_mul_pointwise     │  ← O(N) = 256 operaciones
                          │ ĉᵢ = âᵢ · b̂ᵢ (mod Q)  │
                          └───────────┬───────────┘
                                       │
                                       ▼
                          ĉ = [ĉ₀, ĉ₁, ..., ĉ₂₅₅]
                                       │
                                       ▼
                          ┌──────────────┐
                          │ poly_invntt  │  ← O(N log N)
                          │ + f = 41978  │
                          └──────┬───────┘
                                 │
                                 ▼
              c(x) = a(x) · b(x)  mod (X²⁵⁶ + 1)  mod Q
              c = [c₀, c₁, ..., c₂₅₅]
```

### Coste total vs. método directo

```
Método directo:  256 × 256 = 65 536 multiplicaciones
Método NTT:      2 × 1024 (NTT+INTT butterflies) + 256 (pointwise) + 256 (normalización)
               = 2 560 multiplicaciones (incluyendo montgomery_reduce)

Speedup: 65 536 / 2 560 ≈ 25.6×
```

Este speedup de **25×** es lo que hace que ML-DSA sea práctico en microcontroladores de 32 bits.

---

## 19. Mapa de Constantes — Fase 2

| Constante | Valor          | Por qué existe                                                                |
|-----------|----------------|-------------------------------------------------------------------------------|
| `N`       | `256`          | Grado de los polinomios de ML-DSA. Define la longitud de la NTT.            |
| `ZETA`    | `1753`         | Raíz primitiva 512-ésima de la unidad en `Z_Q`. Genera todas las zetas.      |
| `zetas[]` | 256 valores    | Potencias bit-reversed de ζ en dominio Montgomery. Acceso secuencial O(1).   |
| `f`       | `41978`        | Factor de normalización INTT. `f = N⁻¹ · R² mod Q`. Unifica `1/256` + des-Montgomery. |
| `caddq`   | (función)      | Normalización intermedia en INTT. Previene desbordamiento entre capas.       |

### Relaciones entre constantes

```
ζ^512 ≡ 1  (mod Q)          ← Orden de la raíz
ζ^256 ≡ -1 (mod Q)          ← Propiedad del anillo ciclotómico
zetas[i] = ζ^bitrev(i) · R  (mod Q)   ← Precomputación
f = N⁻¹ · R² = 8 347 681 · 2 365 951 = 41 978  (mod Q)  ← Factor INTT
INTT(NTT(a)) = a            ← Propiedad fundamental
```

---

## 20. Verificación de integridad: el test NTT → INTT

El test en [`tests/test_arith.c`](../tests/test_arith.c) verifica que la cadena `NTT → INTT` es hermética:

```c
for (i = 0; i < 256; i++) {
    a[i] = i;              // Patrón de prueba
    a_orig[i] = a[i];      // Backup
}

poly_ntt(a);                // Transformar al dominio NTT
poly_invntt(a);             // Volver al dominio de coeficientes

for (i = 0; i < 256; i++) {
    if (a[i] != a_orig[i])  // ¿Se recupera el valor original?
        errores++;
}
```

Si `errores == 0`, la NTT y la INTT son inversas exactas, lo que confirma que:
- Las zetas están correctamente precomputadas (bit-reversal + Montgomery).
- La operación butterfly es algebraicamente correcta.
- El factor `f = 41978` normaliza perfectamente.
- No hay desbordamientos en ninguna capa.

---

*Guía de estudio personal — Fases 1 y 2 | `el_pedestal` | ML-DSA bare-metal en C99 de 32 bits*
