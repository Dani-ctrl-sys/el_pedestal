# STUDY_GUIDE.md — Guía de Estudio Completa: Fase 1 de `el_pedestal`

> **Propósito:** Este documento es tu recurso personal de estudio. Explica **todo** lo que ocurre en la Fase 1 del proyecto: qué hace cada pieza, **por qué** la necesitamos, y cómo encajan entre sí. Está pensado para que lo leas, lo releas y te sirva de referencia cuando necesites recordar cualquier concepto.
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

*Guía de estudio personal — Fase 1 | `el_pedestal` | ML-DSA bare-metal en C99 de 32 bits*
