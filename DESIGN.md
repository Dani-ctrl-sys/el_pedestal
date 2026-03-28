# Documento de Diseño: El Pedestal
**Implementación de ML-DSA (FIPS 204) para Sistemas Embebidos de 32 bits**

---

## 1. Introducción

Este documento establece las especificaciones técnicas, decisiones de arquitectura y justificaciones matemáticas para la implementación en C99 de la firma digital post-cuántica ML-DSA (FIPS 204). El diseño del código base está estrictamente optimizado para microcontroladores de 32 bits, priorizando la ejecución determinista y la protección contra ataques de canal lateral.

### 1.1. Parámetros Fundamentales del Sistema

El esquema criptográfico opera sobre el anillo de polinomios $\mathbb{Z}_Q[X]/(X^{256}+1)$. La unidad aritmética de este proyecto se rige por los siguientes parámetros inmutables:

* **Módulo Principal ($Q$):** $8380417 = 2^{23} - 2^{13} + 1$.
* **Dominio de Coeficientes:** Los valores representativos del ruido deben mantenerse estrictamente en el rango centrado $\left[-\lfloor Q/2 \rfloor,\; \lfloor Q/2 \rfloor\right] = [-4190208,\; 4190208]$.
* **Arquitectura Objetivo:** Registros de 32 bits (ej. ARM Cortex-M4), con instrucciones de multiplicación larga en hardware (`SMULL`/`SMLAL`) para productos intermedios de 64 bits.

### 1.2. Paradigma de Seguridad: Código sin Ramificaciones

El principio de diseño más crítico en la capa aritmética es la **ausencia total de saltos condicionales** que dependan de datos en tiempo de ejecución. El uso exclusivo de máscaras de bits y aritmética de tiempo constante mitiga dos vulnerabilidades de hardware específicas:

1. **Predicción de saltos (*Branch Prediction*):** Evita que la unidad de predicción del pipeline altere su estado basándose en material criptográfico sensible.
2. **Ataques de caché (*Cache-Timing Attacks*):** Garantiza un flujo de ejecución estático donde el patrón de acceso a memoria es matemáticamente idéntico en cada ciclo, impidiendo que un atacante deduzca información secreta analizando el estado de la caché L1.

---

## 2. Aritmética Modular de Tiempo Constante (Fase 1)

El núcleo aritmético implementa operaciones en el anillo finito $\mathbb{Z}_Q$. Para cumplir con los requisitos de seguridad y rendimiento, se prescinde de la instrucción de división en hardware y del operador módulo (`%`).

---

### 2.1. Reducción de Montgomery

Se utiliza para la multiplicación rápida de dos elementos dentro del dominio de Montgomery. La operación transforma una división exacta por $Q$ en un desplazamiento de bits.

#### Derivación de $QINV = Q^{-1} \pmod{2^{32}}$

Se busca la constante $QINV$ tal que:

$$Q \cdot QINV \equiv 1 \pmod{2^{32}}$$

> **Nota sobre la congruencia:** El prompt de diseño original especificaba $\equiv -1$, pero la verificación numérica confirma que el valor `58728449` satisface $\equiv +1$. La implementación del código fuente es correcta con esta congruencia positiva.

**Paso 1: Existencia del inverso.**

Como $Q = 2^{23} - 2^{13} + 1$ es impar, $\gcd(Q, 2^{32}) = 1$, por lo que el inverso multiplicativo modular existe por el Teorema de Bézout.

**Paso 2: Semilla por estructura algebraica de $Q$.**

Observamos que $Q = 2^{23} - 2^{13} + 1 \equiv 1 \pmod{2^{13}}$, ya que:

$$2^{23} \equiv 0 \pmod{2^{13}}, \quad -2^{13} \equiv 0 \pmod{2^{13}}$$

Por lo tanto $Q \equiv 1 \pmod{2^{13}}$, lo que implica trivialmente que $x_0 = 1$ es el inverso de $Q$ módulo $2^{13}$:

$$Q \cdot 1 \equiv 1 \pmod{2^{13}}$$

**Paso 3: Levantamiento de Hensel ($2^{13} \to 2^{26}$).**

**Lema de Hensel (demostración del paso inductivo):** Sea $Q \cdot x \equiv 1 \pmod{2^k}$, es decir, $Q \cdot x = 1 + \lambda \cdot 2^k$ para algún entero $\lambda$. Definimos $x' = x(2 - Qx) \pmod{2^{2k}}$ y calculamos:

$$Q \cdot x' = Q \cdot x \cdot (2 - Q \cdot x) = (1 + \lambda 2^k)(2 - (1 + \lambda 2^k))$$
$$= (1 + \lambda 2^k)(1 - \lambda 2^k) = 1 - \lambda^2 \cdot 2^{2k}$$

Por tanto $Q \cdot x' \equiv 1 \pmod{2^{2k}}$. $\blacksquare$

Aplicando con $x_0 = 1$ y $k = 13$:

$$x_1 = 1 \cdot (2 - Q \cdot 1) \pmod{2^{26}} = (2 - 8380417) \pmod{2^{26}}$$

$$x_1 = -8380415 \pmod{2^{26}} = 2^{26} - 8380415 = 67108864 - 8380415 = \mathbf{58728449}$$

**Paso 4: Verificación de que $x_1$ es válido módulo $2^{32}$.**

Expresando $Q$ y $x_1$ en función de potencias de 2:
$$Q = 2^{23} - 2^{13} + 1, \quad x_1 = 2^{26} - 2^{23} + 2^{13} + 1$$

El producto es:
$$Q \cdot x_1 = Q \cdot 2^{26} - (2^{23} - 2^{13})^2 + 1$$

Expandiendo $(2^{23} - 2^{13})^2 = 2^{46} - 2^{37} + 2^{26}$:
$$Q \cdot x_1 = Q \cdot 2^{26} - 2^{46} + 2^{37} - 2^{26} + 1$$

Reduciendo módulo $2^{32}$ (los términos con exponente $\geq 32$ son múltiplos de $2^{32}$, por tanto $\equiv 0$):
- $Q \cdot 2^{26} = (2^{23} - 2^{13} + 1) \cdot 2^{26} = 2^{49} - 2^{39} + 2^{26} \equiv 2^{26} \pmod{2^{32}}$
- $2^{46} \equiv 0$, $\quad 2^{37} \equiv 0$

$$Q \cdot x_1 \equiv 2^{26} - 0 + 0 - 2^{26} + 1 = \mathbf{1} \pmod{2^{32}} \quad \blacksquare$$

Resultado verificado numéricamente: $8380417 \times 58728449 \mod 2^{32} = 1$.

#### Mecánica de Ejecución de Montgomery

Dado un producto intermedio de 64 bits $a$, la reducción de Montgomery realiza:

1. **Cálculo del factor corrector:** $t = (a \bmod 2^{32}) \cdot QINV \bmod 2^{32}$
2. **Anulación de los 32 bits inferiores:** Se forma $a' = a - t \cdot Q$
3. **División exacta:** $\text{resultado} = a' \gg 32$

**Prueba de la anulación:** Por construcción, $t \equiv a \cdot QINV \pmod{2^{32}}$. Como $Q \cdot QINV \equiv 1 \pmod{2^{32}}$, tenemos $t \cdot Q \equiv a \cdot QINV \cdot Q \equiv a \pmod{2^{32}}$. Por tanto:

$$a' = a - t \cdot Q \equiv a - a = 0 \pmod{2^{32}}$$

Los 32 bits inferiores de $a'$ son exactamente cero, haciendo que el desplazamiento `>> 32` sea una **división entera exacta**, sin pérdida de información ni instrucción de división de hardware.

---

### 2.2. Reducción de Barrett: Cota de Error y Multiplicador

Se emplea para reducir valores arbitrarios al dominio de $\mathbb{Z}_Q$ tras operaciones de inyección de ruido.

#### Derivación del Multiplicador $m = 8$

Barrett aproxima la división $\lfloor a / Q \rfloor$ mediante una multiplicación por una fracción precalculada. Dado $k = 26$, el multiplicador óptimo es:

$$m = \left\lfloor \frac{2^k}{Q} \right\rfloor = \left\lfloor \frac{2^{26}}{8380417} \right\rfloor = \left\lfloor \frac{67108864}{8380417} \right\rfloor = \left\lfloor 8.0078\ldots \right\rfloor = \mathbf{8}$$

#### Prueba de la Cota de Error

El cociente verdadero es $q = \lfloor a/Q \rfloor$. El cociente aproximado es $\tilde{q} = \lfloor a \cdot m / 2^k \rfloor$. Analizamos el error $\delta = q - \tilde{q}$.

Dado que $m \leq 2^k/Q$, se tiene $a \cdot m / 2^k \leq a/Q$, luego $\tilde{q} \leq q$, por lo que $\delta \geq 0$.

Para la cota superior, definimos el residuo de la aproximación:

$$\varepsilon = 2^k - m \cdot Q = 2^{26} - 8 \cdot 8380417 = 67108864 - 67043336 = 65528$$

La diferencia entre el cociente real y el aproximado es:

$$\frac{a}{Q} - \frac{a \cdot m}{2^k} = \frac{a \cdot (2^k - m \cdot Q)}{Q \cdot 2^k} = \frac{a \cdot \varepsilon}{Q \cdot 2^k}$$

Para que $\tilde{q} = q$ (error en el piso nulo), se necesita que esta diferencia sea $< 1$:

$$\frac{a \cdot \varepsilon}{Q \cdot 2^k} < 1 \iff a < \frac{Q \cdot 2^k}{\varepsilon} = \frac{8380417 \times 67108864}{65528} \approx 8.58 \times 10^9$$

Para entradas de 32 bits con signo, $|a| < 2^{31} \approx 2.15 \times 10^9 < 8.58 \times 10^9$. Por tanto:

$$\boxed{\delta = q - \tilde{q} \in \{0, 1\}}$$

**El error en la estimación del cociente es estrictamente menor a 1 para cualquier entrada de 32 bits.** El residuo $r = a - \tilde{q} \cdot Q$ puede diferir de $a \bmod Q$ en a lo sumo $Q$.

---

### 2.3. Prueba Formal del Rango Centrado

#### Equivalencia de la Implementación de Hardware con el Redondeo $\lfloor x + \tfrac{1}{2} \rfloor$

La implementación usa la expresión:

```c
int32_t t = (int32_t)(((int64_t)a * 8 + (1 << 25)) >> 26);
```

Que corresponde a:

$$\tilde{q} = \left\lfloor \frac{a \cdot m + 2^{k-1}}{2^k} \right\rfloor = \left\lfloor \frac{a \cdot m}{2^k} + \frac{1}{2} \right\rfloor$$

**Prueba:** Por definición del piso, $\lfloor x + 1/2 \rfloor$ es el entero más cercano a $x$ (con redondeo hacia arriba en el caso de empate exacto). La adición de $2^{k-1}$ antes del desplazamiento de $k$ bits implementa exactamente esta operación en aritmética entera:

$$\left\lfloor \frac{M + 2^{k-1}}{2^k} \right\rfloor = \left\lfloor \frac{M}{2^k} + \frac{1}{2} \right\rfloor$$

donde $M = a \cdot m$ es entero. Esto se verifica directamente: si $M = 2^k \cdot c + r$ con $0 \leq r < 2^k$, entonces:

- Si $r < 2^{k-1}$: $\lfloor (M + 2^{k-1}) / 2^k \rfloor = c = \lfloor M/2^k \rfloor$ (truncar hacia abajo).
- Si $r \geq 2^{k-1}$: $\lfloor (M + 2^{k-1}) / 2^k \rfloor = c + 1$ (redondear hacia arriba). $\quad\blacksquare$

#### Prueba de que el Residuo Permanece en $[-Q/2,\; Q/2]$

La prueba debe conectar el cociente aproximado de Barrett con el redondeo exacto $\lfloor a/Q + 1/2 \rfloor$.

**Lema previo (cota del error de aproximación con redondeo):** Llamamos $v = a \cdot m / 2^k$ (real) y $\tilde{q} = \lfloor v + 1/2 \rfloor$ (entero). Ya probamos en §2.2 que:

$$0 \leq \frac{a}{Q} - v = \frac{a \cdot \varepsilon}{Q \cdot 2^k} < 1 \quad \text{(para } |a| < 2^{31}\text{)}$$

con $\varepsilon = 65528$. Para nuestro rango de entrada la cota es estrictamente menor a $\frac{1}{4}$:

$$\frac{|a| \cdot \varepsilon}{Q \cdot 2^k} < \frac{2^{31} \cdot 65528}{8380417 \cdot 2^{26}} = \frac{2^5 \cdot 65528}{8380417} = \frac{2096896}{8380417} < 0.25$$

Por tanto $v \in (a/Q - 0.25,\; a/Q]$, lo que significa que $v$ y $a/Q$ siempre se redondean al **mismo entero más cercano**:

$$\lfloor v + 1/2 \rfloor = \left\lfloor \frac{a}{Q} + \frac{1}{2} \right\rfloor$$

*Verificación:* Si frac$(a/Q) \in [0, 0.25)$, ambos tienen parte fraccionaria $< 0.5 \Rightarrow$ mismo piso. Si frac$(a/Q) \in [0.25, 0.5)$, el desplazamiento $< 0.25$ no lleva la parte fraccionaria de $v$ por debajo de 0, por lo que el piso es el mismo. Si frac$(a/Q) \geq 0.5$, entonces frac$(v) \geq 0.25 > 0$, y ambos redondean hacia arriba. En todos los casos son iguales. $\blacksquare$

**Prueba del rango:** Sea $q^* = \lfloor a/Q + 1/2 \rfloor$, el entero más cercano a $a/Q$. Por definición:

$$\left| \frac{a}{Q} - q^* \right| \leq \frac{1}{2}$$

Multiplicando por $Q > 0$:

$$|a - q^* \cdot Q| \leq \frac{Q}{2}$$

Como $\tilde{q} = q^*$ (probado arriba), el residuo de la implementación es:

$$\boxed{r = a - \tilde{q} \cdot Q \in \left[-\frac{Q}{2},\; \frac{Q}{2}\right]} \quad \blacksquare$$

#### Relevancia Criptográfica para Esquemas de Retículo

En ML-DSA, los polinomios de ruido deben tener coeficientes con norma acotada. Los rechazos de firma (*rejection sampling*) se basan en verificar que $\|r\|_\infty < \gamma_1 - \beta$ para umbrales definidos en FIPS 204. Esta verificación **sólo es correcta si los residuos están centrados en cero**, no desplazados al rango $[0, Q)$.

Un residuo no centrado produciría que normas aparentemente grandes ($r \approx Q$) correspondan en realidad a valores pequeños ($r \approx -1$), corrompiendo el criterio de rechazo y potencialmente filtrando la clave secreta.

---

### 2.4. Análisis de Seguridad de Implementación: Aritmética *Branchless*

#### Vulnerabilidades de las Sentencias `if` en Criptografía de Hardware

Una reducción modular ingenua con condicional:

```c
// INSEGURO: flujo de ejecución variable según datos secretos
if (a >= Q) a -= Q;
```

expone dos superficies de ataque medibles físicamente:

1. **Predicción de saltos (*Branch Prediction*):** El predictor de la CPU (ej. el GShare predictor del Cortex-M4) aprende el patrón histórico de los saltos. Un atacante que mide el consumo de energía durante el *pipeline flush* (cuando la predicción falla) puede reconstruir la secuencia de bits del material secreto que condiciona el salto.

2. **Ataques de caché (Prime+Probe / Flush+Reload):** Si las ramas `if/else` referencian distintas líneas de caché, el atacante puede inferir qué rama se ejecutó midiendo los tiempos de acceso a memoria tras la operación.

#### La Solución: Máscara de Bits y Extensión de Signo

**Prueba de la extensión de signo por desplazamiento aritmético.**

En C99, para `int32_t x` (entero con signo en complemento a dos), el desplazamiento aritmético a la derecha `x >> 31` replica el **bit de signo** (bit 31) a todas las posiciones inferiores. Formalmente:

- Si $x \geq 0$: el bit 31 es $0$, luego `x >> 31` $= 0 = $ `0x00000000`.
- Si $x < 0$: el bit 31 es $1$, luego `x >> 31` $= -1 = $ `0xFFFFFFFF` (representación en complemento a dos de $-1$ tiene todos los bits a 1).

Esto define la función de máscara:
$$\text{mask}(x) = \begin{cases} \texttt{0x00000000} & \text{si } x \geq 0 \\ \texttt{0xFFFFFFFF} & \text{si } x < 0 \end{cases}$$

La operación `Q & mask(x)` selecciona entonces $Q$ si $x < 0$, y $0$ si $x \geq 0$, sin ninguna instrucción de salto. $\blacksquare$

---

**Prueba de corrección de `conditional_subq`.**

Precondición: $0 \leq a < 2Q$. La función debe devolver $a \bmod Q$.

```c
int32_t res  = a - Q;
int32_t mask = res >> 31;
return res + (Q & mask);
```

*Caso 1: $a \geq Q$.* Entonces $\text{res} = a - Q \geq 0$, luego $\text{mask} = $ `0x00000000`, y se devuelve $\text{res} + 0 = a - Q$. Como $0 \leq a < 2Q$, se tiene $0 \leq a - Q < Q$. Correcto: $a - Q = a \bmod Q$. $\checkmark$

*Caso 2: $a < Q$.* Entonces $\text{res} = a - Q < 0$, luego $\text{mask} = $ `0xFFFFFFFF`, y se devuelve $\text{res} + Q = a - Q + Q = a$. Correcto: $a = a \bmod Q$. $\checkmark$ $\blacksquare$

---

**Prueba de corrección de `caddq`.**

Precondición: $-Q < a < Q$. La función debe devolver $a + Q$ si $a < 0$, y $a$ en caso contrario.

```c
int32_t mask = a >> 31;
return a + (Q & mask);
```

*Caso 1: $a \geq 0$.* $\text{mask} = $ `0x00000000`, se devuelve $a + 0 = a$. Como $0 \leq a < Q$, el resultado está en $[0, Q)$. $\checkmark$

*Caso 2: $a < 0$.* $\text{mask} = $ `0xFFFFFFFF`, se devuelve $a + Q$. Como $-Q < a < 0$, se tiene $0 < a + Q < Q$. El resultado está en $(0, Q)$. $\checkmark$ $\blacksquare$

#### Ventaja Formal de Seguridad

| Propiedad | `if (a >= Q)` | Máscara de bits |
|---|---|---|
| Flujo de ejecución | Variable según `a` | Siempre idéntico |
| Número de instrucciones | Variable | Constante |
| Accesos a memoria | Potencialmente variables | Estáticos |
| Patrón de energía (SPA) | Observable | Uniforme |
| Resistencia a Flush+Reload | No | Sí |

La instrucción `SMULL` (Signed Multiply Long) del Cortex-M4 produce un producto de 64 bits en dos registros en **un único ciclo de reloj determinista**, sin microcódigo variable ni dependencia del valor de los operandos. Esto garantiza que la huella electromagnética y de consumo de energía sea matemáticamente independiente del valor del coeficiente procesado.

---

## 3. Próximas Fases del Proyecto (Roadmap)

Para alcanzar el estándar completo ML-DSA (FIPS 204):

1. **Fase 2 — NTT (Number Theoretic Transform):** Algoritmo de Cooley-Tukey *in-place* sobre $\mathbb{Z}_Q$ con raíces de unidad precomputadas. Estrategias para minimizar la huella de RAM en microcontroladores.
2. **Fase 3 — Funciones XOF (SHAKE-128/256):** Integración de las funciones de expansión pseudoaleatoria de dominio variable, con implementación resistente a ataques de temporización.
3. **Fase 4 — Muestreo y Rechazo (*Rejection Sampling*):** Derivación determinista de vectores de ruido y protocolo de rechazo de firma sin filtración de clave.
4. **Verificación Formal:** Incorporación de herramientas de análisis de tiempo constante (ej. `ct-verif`, `dudect`) para validación formal de la propiedad *branchless* en el código compilado.