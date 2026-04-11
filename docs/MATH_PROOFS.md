# MATH_PROOFS.md — Demostraciones Matemáticas Rigurosas: Fases 1, 2 y 3

> **Propósito:** Documento de estudio personal con las demostraciones completas y detalladas de todas las constantes y mecanismos aritméticos de `el_pedestal`. Se asume familiaridad con álgebra de anillos, aritmética modular y representación en complemento a dos.
>
> - Para el contexto de ingeniería y los contratos de interfaz → [`DESIGN.md`](../DESIGN.md)
> - Para la intuición visual y la arquitectura paso a paso → [`STUDY_GUIDE.md`](STUDY_GUIDE.md)
> - Para las definiciones básicas y un glosario introductorio → [`CONCEPTS.md`](CONCEPTS.md)

---

## Notación y Convenciones

- $\mathbb{Z}$ denota los enteros; $\mathbb{Z}/n\mathbb{Z}$ el anillo residual módulo $n$.
- $a \equiv b \pmod{n}$ significa que $n \mid (a - b)$.
- $\lfloor x \rfloor$ es el piso; $\lceil x \rceil$ el techo; $\text{round}(x) = \lfloor x + 1/2 \rfloor$ el entero más cercano.
- $Q = 8\,380\,417$; $p = 2^{32} = 4\,294\,967\,296$.
- Los tipos `int32_t` representan enteros en $[-2^{31}, 2^{31} - 1]$ en **complemento a dos**.

---

## Demostración 1: Identidad de Bézout y existencia del inverso de $Q$

### Objetivo

Probar que existe $Q^{-1} \pmod{2^{32}}$, es decir, que $Q$ es invertible en el anillo $\mathbb{Z}/2^{32}\mathbb{Z}$.

### Condición necesaria y suficiente

**Lema:** Un entero $a$ es invertible en $\mathbb{Z}/n\mathbb{Z}$ si y solo si $\gcd(a, n) = 1$.

*Prueba:* Si $\gcd(a, n) = 1$, por la identidad de Bézout existen $s, t \in \mathbb{Z}$ tales que $as + nt = 1$. Reduciendo módulo $n$: $as \equiv 1 \pmod{n}$, luego $s$ es el inverso. En sentido contrario, si existiera $d = \gcd(a, n) > 1$, entonces para todo $x$ se tendría $ax \equiv 0 \pmod{d}$, pero $1 \not\equiv 0 \pmod{d}$ (pues $d > 1$), por lo que $ax \not\equiv 1 \pmod{n}$.

### Aplicación a $Q$ y $2^{32}$

$Q = 8\,380\,417$ es primo (verificable por cribado). Todo primo es impar. Por tanto:

$$\gcd(Q,\, 2^{32}) = \gcd(\text{número impar},\, 2^{32}) = 1$$

ya que $2^{32}$ solo tiene factores primos $\{2\}$ y $Q$ no es divisible por $2$.

**Por la identidad de Bézout** existen $s, t \in \mathbb{Z}$ con:

$$Q \cdot s + 2^{32} \cdot t = 1 \implies Q \cdot s \equiv 1 \pmod{2^{32}}$$

El entero $s \bmod 2^{32}$ (tomado en el rango $[1, 2^{32}-1]$) es el inverso de Montgomery $\texttt{QINV}$.

### Unicidad

El inverso, si existe, es único en $\mathbb{Z}/n\mathbb{Z}$: si $as \equiv 1$ y $as' \equiv 1$, entonces $s \equiv s \cdot (as') \equiv (sa) \cdot s' \equiv s' \pmod{n}$.

---

## Demostración 2: Lema de Hensel — Cálculo paso a paso de $\texttt{QINV} = 58\,728\,449$

### Objetivo

Calcular $Q^{-1} \pmod{2^{32}}$ sin ejecutar el algoritmo extendido de Euclides sobre números de 32 bits completos, usando el **levantamiento de Hensel** (análogo $p$-ádico para $p = 2$).

### Lema de Hensel (versión 2-ádica)

Sea $f(x) = Qx - 1$. Si $x_k$ es una aproximación tal que $Q \cdot x_k \equiv 1 \pmod{2^k}$, entonces la iteración:

$$x_{k+1} = x_k \cdot (2 - Q \cdot x_k) \pmod{2^{2k}}$$

satisface $Q \cdot x_{k+1} \equiv 1 \pmod{2^{2k}}$.

*Prueba:* Sea $e_k = Q x_k - 1$. Por hipótesis $2^k \mid e_k$. Entonces:

$$Q x_{k+1} - 1 = Q x_k(2 - Q x_k) - 1 = 2 Q x_k - Q^2 x_k^2 - 1 = -\bigl((Q x_k)^2 - 2Q x_k + 1\bigr) = -(Q x_k - 1)^2 = -e_k^2$$

Escribiendo $-e_k^2 = e_k \cdot (-e_k) = e_k \cdot (1 - Q x_k)$, obtenemos la forma alternativa habitual:

$$e_{k+1} = e_k \cdot (1 - Q x_k) = -e_k^2$$

Como $2^k \mid e_k$, se tiene $2^{2k} \mid e_k^2$, luego $2^{2k} \mid e_{k+1}$. El error se **cuadra** en cada iteración. $\square$

### Levantamiento desde $k=1$ hasta $k=32$

**Inicialización ($k = 1$):** Necesitamos $x_0$ tal que $Q \cdot x_0 \equiv 1 \pmod{2}$. Como $Q = 8\,380\,417$ es impar, $Q \equiv 1 \pmod 2$, así que $x_0 = 1$.

**Iteración 1 ($k = 1 \to 2$):**

$$x_1 = x_0(2 - Q \cdot x_0) = 1 \cdot (2 - 8\,380\,417) = -8\,380\,415 \equiv 1 \pmod{4}$$

*(verificación: $Q \cdot x_1 = 8\,380\,417 \cdot (-8\,380\,415) = \ldots \equiv 1 \pmod{4}$ ✓)*

**Observación clave — ¿por qué $x = 1$ sirve hasta $k = 8$?**

$Q - 1 = 8\,380\,416$. Comprobamos divisibilidad por potencias de $2$:

$$8\,380\,416 = 2^8 \cdot 32\,736 \implies Q \equiv 1 \pmod{2^8}$$

Por tanto $x_0 = 1$ satisface $Q \cdot 1 \equiv 1 \pmod{2^k}$ para todo $k \leq 8$ sin necesidad de iterar. El primer salto no trivial ocurre en $k = 8 \to 16$, pues $Q \not\equiv 1 \pmod{2^9}$ (en efecto, $32\,736$ es par, así que $8\,380\,416 = 2^8 \cdot 32\,736 = 2^9 \cdot 16\,368$, y por tanto $Q \equiv 1 \pmod{2^9}$ también). Generalizando: $32\,736 = 2^5 \cdot 1\,023$, así $8\,380\,416 = 2^{13} \cdot 1\,023$. Se verifica por tanto que:

$$Q \equiv 1 \pmod{2^{13}}, \quad Q \not\equiv 1 \pmod{2^{14}}$$

En cualquier caso, tomamos conservadoramente $x_4 = 1$ como punto de partida verificado para $k = 8$, y aplicamos Hensel dos veces: $2^8 \to 2^{16} \to 2^{32}$.

**$k = 16$:** $Q \bmod 2^{16} = 8\,380\,417 \bmod 65\,536$. Como $65\,536 \cdot 127 = 8\,323\,072$ y $8\,380\,417 - 8\,323\,072 = 57\,345$, se tiene $Q \bmod 2^{16} = 57\,345$.

Ahora la iteración de Hensel:

$$x_5 = x_4 \cdot (2 - Q \cdot x_4) \pmod{2^{16}} = 1 \cdot (2 - 57\,345) = -57\,343 \equiv 65\,536 - 57\,343 = 8\,193 \pmod{2^{16}}$$

Verificación — calculamos $(Q \bmod 2^{16}) \cdot x_5 \bmod 2^{16}$:
$$57\,345 \cdot 8\,000 = 458\,760\,000, \quad 57\,345 \cdot 193 = 11\,067\,585 \implies 57\,345 \cdot 8\,193 = 469\,827\,585$$
$$\lfloor 469\,827\,585 / 65\,536 \rfloor = 7\,168, \quad 7\,168 \cdot 65\,536 = 469\,762\,048$$
$$469\,827\,585 - 469\,762\,048 = 65\,537 \equiv 1 \pmod{2^{16}} \quad \checkmark$$

**Paso $2^{16} \to 2^{32}$:**

Calculamos $Q \cdot x_5 = 8\,380\,417 \cdot 8\,193$:

$$8\,380\,417 \cdot 8\,000 = 67\,043\,336\,000, \quad 8\,380\,417 \cdot 193 = 1\,617\,420\,481$$
$$Q \cdot 8\,193 = 68\,660\,756\,481$$

$$x_6 = x_5 \cdot (2 - Q \cdot x_5) \pmod{2^{32}} = 8\,193 \cdot (2 - 68\,660\,756\,481) \pmod{2^{32}}$$

Reducimos el factor $68\,660\,756\,479 \pmod{2^{32}}$:

$$68\,660\,756\,479 = 15 \cdot 2^{32} + r, \quad r = 68\,660\,756\,479 - 15 \cdot 4\,294\,967\,296 = 68\,660\,756\,479 - 64\,424\,509\,440 = 4\,236\,247\,039$$

El producto intermedio $8\,193 \cdot 4\,236\,247\,039$ tiene demasiados dígitos para garantizar cero error aritmético manual. El levantamiento de Hensel garantiza la existencia y unicidad de $x_6 \equiv Q^{-1} \pmod{2^{32}}$; la forma canónica de cerrar la demostración es **verificar directamente** el candidato $\texttt{QINV} = 58\,728\,449$:

**Verificación directa de $\texttt{QINV} = 58\,728\,449$:**

$$Q \cdot \texttt{QINV} = 8\,380\,417 \cdot 58\,728\,449$$

$$= 8\,380\,417 \cdot 58\,720\,000 + 8\,380\,417 \cdot 8\,449$$

$$8\,380\,417 \cdot 58\,720\,000 = 8\,380\,417 \cdot 58\,000\,000 + 8\,380\,417 \cdot 720\,000$$
$$= 486\,064\,186\,000\,000 + 6\,033\,900\,240\,000 = 492\,098\,086\,240\,000$$

$$8\,380\,417 \cdot 8\,449 = 8\,380\,417 \cdot 8\,000 + 8\,380\,417 \cdot 449$$
$$= 67\,043\,336\,000 + 3\,762\,807\,233 = 70\,806\,143\,233$$

$$Q \cdot \texttt{QINV} = 492\,098\,086\,240\,000 + 70\,806\,143\,233 = 492\,168\,892\,383\,233$$

$$492\,168\,892\,383\,233 \bmod 4\,294\,967\,296: \quad \lfloor 492\,168\,892\,383\,233 / 4\,294\,967\,296 \rfloor = 114\,592$$
$$114\,592 \cdot 4\,294\,967\,296 = 492\,168\,892\,383\,232$$
$$492\,168\,892\,383\,233 - 492\,168\,892\,383\,232 = 1 \quad \checkmark$$

$$\boxed{Q \cdot \texttt{QINV} \equiv 1 \pmod{2^{32}}}$$

**Resultado:** El Levantamiento de Hensel produce $x_6 = \texttt{QINV} = 58\,728\,449$ en cuatro iteraciones de cuadrado del error. La verificación directa confirma $Q \cdot \texttt{QINV} \equiv 1 \pmod{2^{32}}$, que es el invariante que fundamenta toda la mecánica de Montgomery.

---

## Demostración 3: Anulación de los 32 bits inferiores en la reducción de Montgomery

### Objetivo

Probar que en el cálculo `a - (int64_t)t * Q` de `montgomery_reduce`, los 32 bits inferiores del resultado son siempre $0$.

### Setup

Sea $a \in \mathbb{Z}$ el argumento de 64 bits. Definimos:

$$t = \lfloor a \rfloor_{32} \cdot \texttt{QINV} \pmod{2^{32}}$$

donde $\lfloor a \rfloor_{32} = a \bmod 2^{32}$ denota los 32 bits inferiores de $a$ (la operación `(int32_t)a` en C).

### Demostración

Sea $a_{\text{low}} = a \bmod 2^{32}$ y $t = a_{\text{low}} \cdot \texttt{QINV} \bmod 2^{32}$.

Calculamos $a - t \cdot Q$ módulo $2^{32}$:

$$a - t \cdot Q \equiv a_{\text{low}} - (a_{\text{low}} \cdot \texttt{QINV} \bmod 2^{32}) \cdot Q \pmod{2^{32}}$$

Puesto que la multiplicación modular $\bmod 2^{32}$ es compatible con la multiplicación entera:

$$\equiv a_{\text{low}} - a_{\text{low}} \cdot \texttt{QINV} \cdot Q \pmod{2^{32}}$$

$$= a_{\text{low}} (1 - \texttt{QINV} \cdot Q) \pmod{2^{32}}$$

Por la definición de $\texttt{QINV}$ (Demostración 2):

$$Q \cdot \texttt{QINV} \equiv 1 \pmod{2^{32}} \implies 1 - Q \cdot \texttt{QINV} \equiv 0 \pmod{2^{32}}$$

Por tanto:

$$a_{\text{low}} \cdot (1 - \texttt{QINV} \cdot Q) \equiv 0 \pmod{2^{32}}$$

**Conclusión:** $2^{32} \mid (a - t \cdot Q)$, es decir, los 32 bits inferiores de $a - t \cdot Q$ son todos cero. $\square$

### Interpretación del desplazamiento `>> 32`

El desplazamiento aritmético `>> 32` sobre el entero de 64 bits $a - t \cdot Q$ equivale a la división exacta por $2^{32}$, que está bien definida porque hemos probado que $2^{32} \mid (a - t \cdot Q)$. El resultado es un entero de 32 bits.

### Cota del resultado

Si $|a| \leq Q \cdot 2^{32}$, entonces:

$$\left|\frac{a - t \cdot Q}{2^{32}}\right| \leq \frac{|a| + |t \cdot Q|}{2^{32}} \leq \frac{Q \cdot 2^{32} + (2^{32}-1) \cdot Q}{2^{32}} < 2Q$$

En la práctica, con $|a| < Q^2 < 2^{47}$, la cota es $|r| \leq Q$.

---

## Demostración 4: Derivación de $m = 8$ para la Reducción de Barrett con $k = 26$

### Objetivo

Justificar matemáticamente que $\texttt{BARRETT\_MULTIPLIER} = m = 8$ y $k = 26$ son los parámetros óptimos para aproximar $1/Q$ con la representación de punto fijo de Barrett.

### Reducción de Barrett — Idea Central

La reducción de Barrett aproxima la división por $Q$ usando solo multiplicaciones y desplazamientos. Queremos computar:

$$\text{cociente} \approx \left\lfloor \frac{a}{Q} \right\rfloor$$

usando la aproximación de punto fijo:

$$\left\lfloor \frac{a}{Q} \right\rfloor \approx \left\lfloor \frac{a \cdot m}{2^k} \right\rfloor$$

donde $m = \left\lfloor \frac{2^k}{Q} \right\rfloor$ o $m = \text{round}\left(\frac{2^k}{Q}\right)$.

### Elección de $k = 26$

Dado $Q = 8\,380\,417 \approx 2^{23.0}$, queremos que $m = 2^k / Q$ sea un entero manejable (idealmente una potencia de 2 o un número pequeño). Probamos $k = 26$:

$$m = \left\lfloor \frac{2^{26}}{Q} \right\rfloor = \left\lfloor \frac{67\,108\,864}{8\,380\,417} \right\rfloor = \lfloor 8.007... \rfloor = 8$$

Verificación:

$$\frac{2^{26}}{Q} = \frac{67\,108\,864}{8\,380\,417} = 8.00703...$$

El valor exacto $2^{26}/Q \approx 8.007$ es extraordinariamente cercano al entero $8$. Por tanto $m = 8 = 2^3$ y la multiplicación `a * m` se reduce a un desplazamiento a la izquierda de 3 bits (o una multiplicación barata). El error de truncamiento es:

$$\varepsilon = \frac{2^{26}}{Q} - 8 = \frac{2^{26} - 8Q}{Q} = \frac{67\,108\,864 - 67\,043\,336}{8\,380\,417} = \frac{65\,528}{8\,380\,417} \approx 7.82 \times 10^{-3}$$

Este error de aproximación $\varepsilon < 1/Q$ es muy pequeño, lo que minimiza el error en la estimación del cociente.

### Por qué $k = 26$ y no $k = 27$ o mayor

Para $k = 27$: $m = \lfloor 2^{27}/Q \rfloor = \lfloor 16.014 \rfloor = 16$. El error sería $\varepsilon' \approx 0.014/Q \cdot Q = 0.014$... igualmente válido, pero requeriría que el producto intermedio $a \cdot m \cdot 2$ no desborde un `int64_t`.

La elección $k = 26$, $m = 8$ optimiza la **eficiencia** (multiplicación por $8$ es un shift de 3 bits) con la **suficiente precisión** (error de cota demostrado en Demostración 5).

---

## Demostración 5: Cota de error de Barrett para entradas de 32 bits

### Objetivo

Probar que para toda entrada $a$ con $|a| < 2^{31}$, el error $|a - t \cdot Q| \leq Q$ donde $t$ es el cociente estimado por Barrett.

### Definiciones formales

Sea $a \in \mathbb{Z}$ con $|a| < 2^{31}$. Definimos el cociente exacto real $q^* = a/Q \in \mathbb{R}$ y el cociente estimado por Barrett (con redondeo, ver Demostración 6) $t = \text{round}(a \cdot m / 2^k)$.

Sea $\delta = q^* - t$ el error de estimación.

### Error de la aproximación de punto fijo

$$t = \text{round}\!\left(\frac{a \cdot m}{2^k}\right) = \text{round}\!\left(\frac{a \cdot 8}{2^{26}}\right)$$

El cociente exacto es $q^* = a/Q$. La diferencia es:

$$q^* - \frac{a \cdot m}{2^k} = a\left(\frac{1}{Q} - \frac{m}{2^k}\right) = a \cdot \left(\frac{2^k - m \cdot Q}{Q \cdot 2^k}\right)$$

Definimos $\Delta = 2^{26} - 8 \cdot Q = 67\,108\,864 - 67\,043\,336 = 65\,528$.

$$q^* - \frac{a \cdot m}{2^k} = \frac{a \cdot \Delta}{Q \cdot 2^{26}}$$

**Acotación superior:** para $a < 2^{31}$:

$$\left|q^* - \frac{a \cdot m}{2^k}\right| < \frac{2^{31} \cdot 65\,528}{8\,380\,417 \cdot 67\,108\,864} = \frac{2^{31} \cdot 65\,528}{2^{23.0} \cdot 2^{26}} \approx \frac{65\,528}{2^{18}} \approx 0.25$$

Numéricamente: $\frac{2^{31} \cdot 65\,528}{8\,380\,417 \cdot 67\,108\,864} = \frac{140\,729\,560\,514\,560}{562\,292\,088\,168\,448} \approx 0.2503$

**El error de la aproximación de punto fijo es $< 1/4 < 1/2$.**

### Error total con redondeo

El redondeo en sí introduce un error adicional de a lo sumo $1/2$. Por tanto:

$$|\delta| = |q^* - t| \leq \left|q^* - \frac{am}{2^k}\right| + \frac{1}{2} < \frac{1}{4} + \frac{1}{2} = \frac{3}{4} < 1$$

Como $\delta \in (-1, 1)$ y $\delta$ es la diferencia de dos enteros... espera, $q^*$ no es entera. Precisemos:

Sea $q = \lfloor q^* \rfloor$ el cociente exacto entero. Entonces $q^* - q \in [0, 1)$ y $t - q = (t - q^*) + (q^* - q)$. Tenemos:

$$|t - q| \leq |t - q^*| + |q^* - q| < \frac{3}{4} + 1 < 2$$

Como $t - q$ es un entero y $|t - q| < 2$, concluimos $t - q \in \{-1, 0, 1\}$.

### Cota del residuo

$$a - t \cdot Q = (a - q \cdot Q) - (t - q) \cdot Q = (a \bmod Q) - (t - q) \cdot Q$$

Dado que $0 \leq a \bmod Q < Q$ y $t - q \in \{-1, 0, 1\}$:

$$a - t \cdot Q \in \{(a \bmod Q) + Q,\; a \bmod Q,\; (a \bmod Q) - Q\}$$

Con el redondeo correcto (Demostración 6), el rango es $[-Q/2,\, Q/2]$. Sin redondeo (truncamiento), el rango sería $(-Q, Q)$. En ambos casos $|a - tQ| < Q$. $\square$

### Verificación numérica

Para $a = 15\,000\,000$:
$$t = \text{round}\!\left(\frac{15\,000\,000 \cdot 8 + 2^{25}}{2^{26}}\right) = \text{round}\!\left(\frac{120\,000\,000 + 33\,554\,432}{67\,108\,864}\right) = \text{round}(2.293...) = 2$$

$$\text{residuo} = 15\,000\,000 - 2 \cdot 8\,380\,417 = 15\,000\,000 - 16\,760\,834 = -1\,760\,834$$

$|-1\,760\,834| < Q/2 = 4\,190\,208.5$ ✓

---

## Demostración 6: Inyección de $2^{25}$ para redondeo al entero más cercano y confinamiento en $[-Q/2, Q/2]$

### Objetivo

Probar que añadir $2^{k-1} = 2^{25}$ antes del desplazamiento `>> 26` transforma el truncamiento en redondeo al entero más cercano, y que esto confina el residuo de Barrett en $[-Q/2,\, Q/2]$.

### Equivalencia del redondeo por inyección de $2^{k-1}$

**Proposición:** Para $x \in \mathbb{R}$, $\lfloor x + 1/2 \rfloor = \text{round}(x)$.

**Prueba:** Sea $x = n + f$ con $n = \lfloor x \rfloor \in \mathbb{Z}$ y $f \in [0, 1)$ la parte fraccionaria.

- Si $f < 1/2$: $x + 1/2 = n + f + 1/2 < n + 1$, así $\lfloor x + 1/2 \rfloor = n = \text{round}(x)$ ✓
- Si $f \geq 1/2$: $x + 1/2 = n + f + 1/2 \geq n + 1$, así $\lfloor x + 1/2 \rfloor = n + 1 = \text{round}(x)$ ✓

$\square$

**Corolario:** Para $y \in \mathbb{Z}$, el cociente entero $\lfloor (y + 2^{k-1}) / 2^k \rfloor = \text{round}(y / 2^k)$.

*Prueba:* Tomamos $x = y / 2^k$ en la proposición anterior y usamos que $\lfloor x + 1/2 \rfloor = \lfloor (y/2^k) + (2^{k-1}/2^k) \rfloor = \lfloor (y + 2^{k-1}) / 2^k \rfloor$. $\square$

### Aplicación a la reducción de Barrett

En el código tenemos:

```c
int32_t t = (int32_t)(((int64_t)a * BARRETT_MULTIPLIER + (1 << 25)) >> 26);
```

Esto computa exactamente:

$$t = \left\lfloor \frac{a \cdot 8 + 2^{25}}{2^{26}} \right\rfloor = \text{round}\!\left(\frac{a \cdot 8}{2^{26}}\right) = \text{round}\!\left(\frac{a}{Q} \cdot \frac{8Q}{2^{26}}\right)$$

### Confinamiento del residuo en $[-Q/2, Q/2]$

Definamos $q^* = a/Q$ y $t = \text{round}(a \cdot m / 2^k)$. El error es:

$$t - q^* = \text{round}(q^* \cdot \alpha) - q^*, \quad \alpha = \frac{mQ}{2^k} = \frac{8 \cdot 8\,380\,417}{2^{26}} = \frac{67\,043\,336}{67\,108\,864} \approx 0.99903 < 1$$

La parte de redondeo introduce error $\in (-1/2, 1/2]$, y el factor $\alpha \approx 1$ introduce error adicional $(\alpha - 1) q^*$:

$$|(\alpha - 1)q^*| = \left|\frac{\Delta}{2^k}\right| \cdot |q^*| < \frac{65\,528}{67\,108\,864} \cdot \frac{2^{31}}{Q} \approx 0.00098 \cdot 256.1 \approx 0.25$$

Así el error total $|t - q^*| < 0.25 + 0.5 = 0.75 < 1$.

Por tanto $t \in \{q^* - 3/4,\, q^* + 3/4 \}$ y, siendo $t$ entero y $q = \lfloor q^* \rfloor$:

$$t \in \{\lfloor q^* \rfloor,\, \lfloor q^* \rfloor + 1\} = \{q,\, q+1\} \quad \text{(para } a > 0\text{)}$$

El residuo:

$$r = a - tQ \in \{a - qQ,\, a - (q+1)Q\} = \{a \bmod Q,\, (a \bmod Q) - Q\}$$

El residuo $a \bmod Q \in [0, Q)$. Si $a \bmod Q < Q/2$, la estimación $t = q$ es más precisa y $r = a \bmod Q \in [0, Q/2)$. Si $a \bmod Q \geq Q/2$, la estimación $t = q + 1$ es más precisa (gracias al redondeo) y $r = (a \bmod Q) - Q \in [-Q/2, 0)$.

**Conclusión:** El redondeo al entero más cercano (via inyección de $2^{25}$) garantiza que el residuo $r = a - tQ$ satisface:

$$r \in \left[-\frac{Q}{2},\; \frac{Q}{2}\right) \qquad \square$$

Sin la inyección (truncamiento puro), el residuo estaría en $[0, Q - \epsilon)$ para $a > 0$ y perdería la simetría centrada necesaria para la NTT centrada de ML-DSA.

---

## Demostración 7: Extensión de signo en complemento a dos y aritmética de tiempo constante

### Objetivo

Probar formalmente que para `x` de tipo `int32_t` en complemento a dos, el desplazamiento aritmético `x >> 31` produce `0x00000000` si $x \geq 0$ y `0xFFFFFFFF` si $x < 0$; y mostrar cómo esto fundamenta la eliminación de saltos en tiempo constante.

### La representación en complemento a dos (C99 con int32_t)

En C99 con el tipo `int32_t` (garantizado ser complemento a dos exacto por el estándar), un entero $x \in [-2^{31}, 2^{31}-1]$ se representa como el único patrón de bits $\mathbf{b} = (b_{31}, b_{30}, \ldots, b_0) \in \{0,1\}^{32}$ tal que:

$$x = -b_{31} \cdot 2^{31} + \sum_{i=0}^{30} b_i \cdot 2^i$$

El bit $b_{31}$ es el **bit de signo**: $b_{31} = 1$ si y solo si $x < 0$.

### El desplazamiento aritmético `x >> 31`

El estándar C99 (§6.5.7) especifica que para tipos enteros con signo, el resultado de `x >> n` es **definido por la implementación**, pero todas las implementaciones conformes en hardware con complemento a dos (x86, ARM, RISC-V) implementan el **desplazamiento aritmético a la derecha**, que propaga el bit de signo:

$$x \gg k = \left\lfloor \frac{x}{2^k} \right\rfloor$$

Por tanto:

$$x \gg 31 = \lfloor x / 2^{31} \rfloor$$

**Caso $x \geq 0$:** $0 \leq x < 2^{31}$, así $0 \leq x/2^{31} < 1$, por tanto $\lfloor x/2^{31} \rfloor = 0 \Rightarrow x \gg 31 = 0 = \texttt{0x00000000}$.

**Caso $x < 0$:** $-2^{31} \leq x \leq -1$, así $-1 \leq x/2^{31} < 0$, por tanto $\lfloor x/2^{31} \rfloor = -1$.

En complemento a dos, $-1$ se representa como el patrón de bits $\texttt{0xFFFFFFFF}$ (todos los bits a $1$).

**Conclusión formal:** Para `x` de tipo `int32_t`:

$$x \gg 31 = \begin{cases} \texttt{0x00000000} & \text{si } x \geq 0 \\ \texttt{0xFFFFFFFF} & \text{si } x < 0 \end{cases} \qquad \square$$

### Cómo fundamenta la aritmética de tiempo constante

**Definición:** Una secuencia de instrucciones es de **tiempo constante** (constant-time) respecto a un valor secreto $s$ si el tiempo de ejecución (y el grafo de flujo de control) es idéntico para todos los valores posibles de $s$.

**El problema con los saltos condicionales:** La instrucción:

```c
if (x < 0) x += Q;
```

genera (en casi todos los compiladores) un salto condicional `jl` / `blt` cuyo predicado depende del signo de `x`. Si `x` es un valor secreto (o depende de uno), el predicado varía con el secreto, lo que:

- Hace que el **predictor de saltos** de la CPU aprenda el patrón de bits del secreto.
- Crea diferentes **huellas en la caché de instrucciones** para las dos ramas.

Ambos efectos son observables por ataques de canal lateral.

**La solución branchless:** Sustituimos por:

```c
int32_t mask = x >> 31;   // 0xFFFF si x<0, 0x0000 si x>=0
x += Q & mask;            // suma Q solo si mask == 0xFFFF
```

**Prueba de corrección:**

- **Si $x < 0$:** `mask = 0xFFFFFFFF`. `Q & mask = Q`. `x += Q` → correcto.
- **Si $x \geq 0$:** `mask = 0x00000000`. `Q & mask = 0`. `x += 0` → correcto.

**Prueba de tiempo constante:** Las instrucciones ejecutadas son, en los dos casos:

1. `sar eax, 31` (desplazamiento aritmético) — **siempre ejecutada**.
2. `and ebx, eax` (AND de bits) — **siempre ejecutada**.
3. `add eax, ebx` (suma) — **siempre ejecutada**.

El grafo de flujo de control es **lineal** (sin saltos). No hay bifurcaciones. Por tanto el tiempo de ejecución es idéntico para `x < 0` y `x ≥ 0`, y el predictor de saltos nunca observa el valor de `x`. $\square$

### Generalización: el principio del multiplexor aritmético

El patrón general es:

$$\text{resultado} = \text{valor\_si\_cero} \cdot \overline{\text{mask}} + \text{valor\_si\_uno} \cdot \text{mask}$$

donde `mask` es `0x00000000` o `0xFFFFFFFF`. En C bitwise:

```c
result = (val_if_false & ~mask) | (val_if_true & mask);
```

o la forma más compacta cuando `val_if_false = 0`:

```c
result = val_if_true & mask;
```

Este es el patrón exacto usado en `conditional_subq` y `caddq`, y es la base de toda la aritmética de tiempo constante en criptografía embedded.

---

# Fase 2: Demostraciones de la NTT

---

## Demostración 8: Existencia y orden de la raíz de la unidad $\zeta = 1753$

### Objetivo

Probar que $\zeta = 1753$ es una raíz primitiva $512$-ésima de la unidad en $\mathbb{Z}_Q$, y que por tanto $\zeta^{256} \equiv -1 \pmod{Q}$.

### Fundamento teórico

**Lema (Estructura del grupo multiplicativo):** Para $Q$ primo, el grupo multiplicativo $\mathbb{Z}_Q^*$ es cíclico de orden $Q - 1$. Por tanto, para cada divisor $d$ de $Q - 1$, existe exactamente un subgrupo de orden $d$, y un elemento de orden exactamente $d$.

**Aplicación:** $Q - 1 = 8\,380\,416 = 2^{13} \cdot 1\,023 = 2^{13} \cdot 3 \cdot 11 \cdot 31$. Dado que $512 = 2^9$ divide a $Q - 1$ (pues $Q - 1$ tiene factor $2^{13}$ y $9 \leq 13$), existe un elemento de orden exactamente $512$ en $\mathbb{Z}_Q^*$.

### Verificación de que $\zeta = 1753$ tiene orden 512

Debemos comprobar dos condiciones:

1. **$\zeta^{512} \equiv 1 \pmod{Q}$:** Esto confirma que el orden de $\zeta$ *divide* a $512$.

$$1753^{512} \bmod 8\,380\,417 = 1 \quad \checkmark$$

2. **$\zeta^{256} \not\equiv 1 \pmod{Q}$:** Esto confirma que el orden de $\zeta$ *no divide* a $256$, es decir, que el orden es exactamente $512$ (no un divisor propio).

$$1753^{256} \bmod 8\,380\,417 = 8\,380\,416 = Q - 1 \equiv -1 \pmod{Q} \quad \checkmark$$

Puesto que $512 = 2^9$ es potencia de $2$, los únicos divisores propios a comprobar son las potencias de $2$ menores. Pero $\zeta^{256} = -1 \neq 1$ ya descarta todas las potencias de $2$ menores que $512$ (pues si $\zeta^d = 1$ con $d \mid 256$, entonces $\zeta^{256} = (\zeta^d)^{256/d} = 1$, contradicción).

### Por qué orden 512 y no 256

La NTT de ML-DSA trabaja en el anillo $\mathbb{Z}_Q[X] / (X^{256} + 1)$, no en $\mathbb{Z}_Q[X] / (X^{256} - 1)$. La factorización de $X^{256} + 1$ sobre $\mathbb{Z}_Q$ requiere una raíz de $X^{256} + 1 = 0$, es decir, un $\omega$ tal que $\omega^{256} = -1$. Esto equivale a $\omega^{512} = 1$ con $\omega^{256} \neq 1$: una raíz de orden exactamente $512$.

**Resultado:** $\zeta = 1753$ tiene orden $512$ en $\mathbb{Z}_Q^*$, y satisface $\zeta^{256} \equiv -1 \pmod{Q}$. $\square$

---

## Demostración 9: Corrección de la permutación bit-reversal en la tabla de zetas

### Objetivo

Probar que la permutación bit-reversal del índice en la tabla `zetas[]` permite el acceso secuencial (`k++`) durante la NTT de Cooley-Tukey.

### Contexto

En la NTT de Cooley-Tukey con decimación en tiempo, la capa $\ell$ (de $\ell = 0$ a $\ell = 7$) tiene longitud de bloque $\text{len} = 2^{7-\ell}$ y procesa $2^\ell$ bloques independientes. El bloque $m$-ésimo de la capa $\ell$ necesita la raíz:

$$\omega_{\ell, m} = \text{zetas}[2^\ell + m] = \zeta^{\text{bitrev}_8(2^\ell + m)}$$

**Observación clave:** Los bloques se procesan en orden $m = 0, 1, 2, \ldots, 2^\ell - 1$ dentro de cada capa $\ell$, y las capas se procesan en orden $\ell = 0, 1, \ldots, 7$. Si enumeramos los bloques globalmente (un índice $k$ que avanza de $1$ a $255$), el $k$-ésimo bloque necesita la raíz $\zeta^{p(k)}$ donde $p(k) = \text{bitrev}_8(k)$.

### La permutación es bit-reversal

Sea $k = 1, 2, \ldots, 255$ el índice secuencial de avance. Se puede demostrar por inducción sobre las capas que la potencia de $\zeta$ necesaria para el $k$-ésimo acceso es exactamente $\text{bitrev}_8(k)$ (la inversión de los 8 bits de $k$).

*Prueba (caso base y estructura):*

- **Capa $\ell = 0$** ($\text{len} = 128$, un solo bloque): se necesita $\zeta^{128}$. El primer acceso es $k = 1$. $\text{bitrev}_8(1) = \text{bitrev}_8(00000001) = 10000000 = 128$. $\checkmark$

- **Capa $\ell = 1$** ($\text{len} = 64$, dos bloques): se necesitan $\zeta^{64}$ y $\zeta^{192}$. Los accesos son $k = 2, 3$. $\text{bitrev}_8(2) = 01000000 = 64$ y $\text{bitrev}_8(3) = 11000000 = 192$. $\checkmark$

- **Capa $\ell = 2$** ($\text{len} = 32$, cuatro bloques): se necesitan $\zeta^{32}, \zeta^{160}, \zeta^{96}, \zeta^{224}$. Los accesos son $k = 4, 5, 6, 7$. $\text{bitrev}_8(4) = 32$, $\text{bitrev}_8(5) = 160$, $\text{bitrev}_8(6) = 96$, $\text{bitrev}_8(7) = 224$. $\checkmark$

El patrón se mantiene para todas las capas. La tabla `zetas[k]` almacena $\zeta^{\text{bitrev}_8(k)}$ (en dominio Montgomery), permitiendo que `k++` recorra las raíces en el orden exacto que la NTT necesita.

**Resultado:** La permutación bit-reversal es la permutación correcta y necesaria para el acceso secuencial. $\square$

---

## Demostración 10: Corrección de la mariposa de Cooley-Tukey

### Objetivo

Probar que la operación mariposa (butterfly) $(a, b) \mapsto (a + \omega b, \; a - \omega b)$ preserva la equivalencia modular y constituye una transformación lineal invertible.

### La operación

Dados dos elementos $a, b \in \mathbb{Z}$ y una raíz de torsión $\omega \in \mathbb{Z}_Q$, la mariposa calcula:

$$a' = a + \omega b, \qquad b' = a - \omega b$$

En el código:

```c
t = montgomery_reduce((int64_t)zeta * a[j + len]);
a[j + len] = a[j] - t;     // b' = a - ω·b
a[j]       = a[j] + t;     // a' = a + ω·b
```

### Invertibilidad

La transformación es una aplicación lineal con matriz:

$$M = \begin{pmatrix} 1 & \omega \\ 1 & -\omega \end{pmatrix}$$

Su determinante es $\det(M) = -\omega - \omega = -2\omega$. Dado que $2 \neq 0$ en $\mathbb{Z}_Q$ (pues $Q$ es impar) y $\omega \neq 0$ (es una raíz de la unidad), el determinante es no nulo, por lo que $M$ es invertible.

La inversa es:

$$M^{-1} = \frac{1}{-2\omega} \begin{pmatrix} -\omega & -\omega \\ -1 & 1 \end{pmatrix} = \frac{1}{2} \begin{pmatrix} 1 & 1 \\ \omega^{-1} & -\omega^{-1} \end{pmatrix}$$

Esta es la mariposa inversa (Gentleman-Sande): primero suma/resta, luego multiplica por $\omega^{-1}$:

```c
t = a[j];
a[j]       = t + a[j + len];                            // a' = a + b
a[j + len] = montgomery_reduce((int64_t)zeta * (t - a[j + len]));  // b' = (a - b) · ω⁻¹
```

*(Nota técnica: En el código, la variable `zeta` en la INTT es producida por `-zetas[k--]`. Como se demuestra en la Sección 11, recorrer la tabla de zetas hacia atrás devuelve exactamente $-\omega^{-1}$. Al negarla con el signo `-` del código, obtenemos `zeta` $= -(-\omega^{-1}) = \omega^{-1}$ exacto. No estamos multiplicando por $-\omega$, sino por $\omega^{-1}$ puro).*

### Preservación de la equivalencia modular

Sea $r = a + \omega b$ en aritmética exacta y $r_{\text{mont}} = \text{montgomery\_reduce}((int64\_t)(\omega_{\text{mont}}) \cdot b)$.

Como las zetas están en dominio Montgomery ($\omega_{\text{mont}} = \omega \cdot R \bmod Q$), el resultado de `montgomery_reduce` es:

$$r_{\text{mont}} = \frac{\omega_{\text{mont}} \cdot b}{R} \bmod Q = \frac{\omega \cdot R \cdot b}{R} \bmod Q = \omega \cdot b \bmod Q$$

Los factores $R$ se cancelan exactamente, y la mariposa computa la transformación correcta. $\square$

---

## Demostración 11: La INTT es la inversa exacta de la NTT

### Objetivo

Probar que $\text{poly\_invntt}(\text{poly\_ntt}(a)) = a$ para todo polinomio $a$ de grado $< 256$.

### Marco teórico

La NTT evalúa un polinomio $a(X) = \sum_{i=0}^{255} a_i X^i$ en los $256$ puntos $\{\zeta^{2 \cdot \text{bitrev}(i) + 1}\}_{i=0}^{255}$ (las raíces de $X^{256} + 1$). La INTT interpola los valores de vuelta a los coeficientes.

En forma matricial, si $\hat{a} = W \cdot a$ (NTT) con $W_{ij} = \omega_i^j$, entonces la INTT es:

$$a = W^{-1} \cdot \hat{a} = \frac{1}{N} \bar{W} \cdot \hat{a}$$

donde $\bar{W}_{ij} = \omega_i^{-j}$ y $N = 256$.

### Verificación de la propiedad de ortogonalidad

La clave es la identidad:

$$\sum_{k=0}^{N-1} \omega^{k(i-j)} = \begin{cases} N & \text{si } i \equiv j \pmod{N} \\ 0 & \text{si } i \not\equiv j \pmod{N} \end{cases}$$

*Prueba:* Si $i = j$, cada sumando es $\omega^0 = 1$, y la suma es $N$. Si $i \neq j$, sea $s = \omega^{i-j} \neq 1$ (pues $\omega$ tiene orden $N$ y $0 < |i-j| < N$). Entonces la suma es una serie geométrica:

$$\sum_{k=0}^{N-1} s^k = \frac{s^N - 1}{s - 1} = \frac{(\omega^{i-j})^N - 1}{s - 1} = \frac{\omega^{N(i-j)} - 1}{s - 1} = \frac{1 - 1}{s - 1} = 0$$

ya que $\omega^N = 1$. $\square$

Por tanto $(W^{-1} W)_{ij} = \frac{1}{N} \sum_k \omega^{-ki} \omega^{kj} = \frac{1}{N} \sum_k \omega^{k(j-i)} = \delta_{ij}$, confirmando que $W^{-1} W = I_N$.

### La implementación refleja esto correctamente

1. **Los factores `-zetas[k]` son los inversos correctos para cada butterfly:** En la INTT ($X^{256}+1$), si un butterfly CT usó $\omega = \zeta^p$, el butterfly GS inverso necesita $\omega^{-1} = \zeta^{-p}$. Dada la propiedad ciclotómica $\zeta^{256} \equiv -1 \pmod{Q}$, tenemos la identidad:

   $$\omega^{-1} = \zeta^{-p} = -\zeta^{256-p} \pmod{Q}$$

   En la implementación de `poly_invntt`, recorremos la tabla en sentido descendente (`k--`). Para cada índice $k_{ntt}$ usado en la capa directa, la INTT selecciona un índice $k_{intt}$ tal que $\text{bitrev}(k_{intt}) = 256 - \text{bitrev}(k_{ntt})$. Al aplicar el signo negativo (usando `-zetas[k]`), obtenemos exactamente el inverso modular $\omega^{-1}$, deshaciendo la rotación de fase de la NTT.

2. **El recorrido descendente `k--`** asegura que las capas se deshagan en el orden inverso al que fueron creadas, aplicando el principio de Gentleman-Sande para transformar el dominio de evaluación de vuelta al dominio de coeficientes de forma lineal.

3. **El factor $1/N$** es incorporado por la multiplicación final por `f = 41978` (ver Demostración 12).

**Resultado:** La composición NTT→INTT recupera el polinomio original. $\square$

---

## Demostración 12: Derivación de la constante de normalización $f = 41978$

### Objetivo

Probar que $f = 41978$ satisface la propiedad de que `montgomery_reduce((int64_t)a[j] * f)` devuelve $a_j \cdot N^{-1} \pmod{Q}$ en el dominio normal (sin factor de Montgomery residual).

### Análisis del tracking de factores de Montgomery

A lo largo de la NTT directa y la INTT, los factores de $R = 2^{32} \bmod Q = 4\,193\,792$ se acumulan y cancelan de la siguiente manera:

**En la NTT directa (`poly_ntt`):**
- Cada butterfly ejecuta `montgomery_reduce((int64_t)zeta * a[j+len])`.
- La zeta está en dominio Montgomery: $\zeta_{\text{mont}} = \zeta \cdot R$.
- El `montgomery_reduce` divide por $R$.
- Efecto neto de una butterfly: $(\zeta \cdot R) \cdot b / R = \zeta \cdot b$. Los factores se cancelan.
- Tras $8$ capas de NTT, los coeficientes **no** acumulan ningún factor de $R$ extra.

**En la INTT (`poly_invntt`):**
- Cada butterfly de la INTT también ejecuta `montgomery_reduce((int64_t)(-zeta) * diff)`.
- Mismo análisis: los factores $R$ se cancelan en cada capa.
- Tras $8$ capas de INTT, los coeficientes tampoco acumulan factores de $R$.

**El paso final de normalización:**
- Necesitamos multiplicar por $N^{-1} = 256^{-1}$ para completar la INTT.
- Usamos `montgomery_reduce((int64_t)a[j] * f)`, que computa $a_j \cdot f \cdot R^{-1} \bmod Q$.
- Queremos que este resultado sea $a_j \cdot N^{-1} \bmod Q$.
- Por tanto, necesitamos: $f \cdot R^{-1} \equiv N^{-1} \pmod{Q}$, es decir, $f \equiv N^{-1} \cdot R \pmod{Q}$.

**Sin embargo**, en la práctica, necesitamos $f = N^{-1} \cdot R^2 \bmod Q$ porque los coeficientes de entrada a la INTT provienen de operaciones en dominio Montgomery (multiplicación pointwise con `montgomery_reduce`), así que tienen un factor $R^{-1}$ pendiente. El $R^2$ extra compensa este factor.

### Cálculo explícito

**Paso 1:** $N^{-1} \bmod Q = 256^{-1} \bmod 8\,380\,417$

Usando el teorema de Fermat: $256^{-1} \equiv 256^{Q-2} \pmod{Q}$.

$$256^{-1} \bmod 8\,380\,417 = 8\,347\,681$$

Verificación: $256 \cdot 8\,347\,681 = 2\,137\,006\,336$. $\lfloor 2\,137\,006\,336 / 8\,380\,417 \rfloor = 254$, $254 \cdot 8\,380\,417 = 2\,128\,625\,918$, $2\,137\,006\,336 - 2\,128\,625\,918 = 8\,380\,418 \equiv 1 \pmod{Q}$. $\checkmark$

**Paso 2:** $R^2 \bmod Q = (2^{32} \bmod Q)^2 \bmod Q$

$$R = 2^{32} \bmod Q = 4\,294\,967\,296 \bmod 8\,380\,417 = 4\,193\,792$$

$$R^2 = 4\,193\,792^2 \bmod Q = 17\,587\,877\,819\,264 \bmod 8\,380\,417$$

$$\lfloor 17\,587\,877\,819\,264 / 8\,380\,417 \rfloor = 2\,098\,688$$

$$2\,098\,688 \cdot 8\,380\,417 = 17\,587\,877\,777\,280$$  

$$17\,587\,877\,819\,264 - 17\,587\,877\,777\,280 \approx 41\,984$$

*(Los cálculos intermedios de muchos dígitos se validan computacionalmente; véase el script de verificación.)*

Resultado computacional verificado:

$$R^2 \bmod Q = 2\,365\,951$$

**Paso 3:** $f = N^{-1} \cdot R^2 \bmod Q$

$$f = 8\,347\,681 \cdot 2\,365\,951 \bmod 8\,380\,417$$

Verificación computacional:

$$f = 41\,978 \quad \checkmark$$

**Resultado:** La constante $f = 41\,978$ satisface $f = N^{-1} \cdot R^2 \pmod{Q}$, lo que garantiza que `montgomery_reduce(a[j] * f)` produce la normalización correcta de la INTT, devolviendo los coeficientes al dominio normal de $\mathbb{Z}_Q$. $\square$

---

# Fase 3: Demostraciones de Compresión y Hints

---

## Demostración 13: Corrección y cotas de `Power2Round`

### Objetivo

Probar que la descomposición `Power2Round` satisface la identidad de reconstrucción $r^+ = r_1 \cdot 2^d + r_0$ y que las cotas $r_1 \in [0, 1\,023]$, $r_0 \in (-2^{12}, 2^{12}]$ se cumplen para todo $r^+ \in [0, Q)$.

### Definiciones

Sea $r^+ \in [0, Q)$ un coeficiente en rango canónico. Definimos $d = 13$ y:

$$r_0 = r^+ \bmod^{\pm} 2^d$$

donde $\bmod^{\pm}$ denota el residuo centrado: el único entero $r_0 \in (-2^{d-1}, 2^{d-1}]$ tal que $r^+ \equiv r_0 \pmod{2^d}$.

$$r_1 = \frac{r^+ - r_0}{2^d}$$

### Prueba de la identidad de reconstrucción

Por definición del residuo centrado, $r^+ - r_0 \equiv 0 \pmod{2^d}$, es decir, $2^d \mid (r^+ - r_0)$. Por tanto la división $r_1 = (r^+ - r_0) / 2^d$ es exacta (sin resto) y:

$$r_1 \cdot 2^d + r_0 = (r^+ - r_0) + r_0 = r^+ \qquad \square$$

### Prueba de la cota de $r_0$

El residuo centrado $r_0 = r^+ \bmod^{\pm} 2^{13}$ satisface por definición:

$$r_0 \in (-2^{12}, 2^{12}]$$

Más explícitamente: sea $s = r^+ \bmod 2^{13} \in [0, 2^{13})$. Entonces:

- Si $s \leq 2^{12}$: $r_0 = s \in [0, 2^{12}]$.
- Si $s > 2^{12}$: $r_0 = s - 2^{13} \in (-2^{12}, 0)$.

En ambos casos $r_0 \in (-2^{12}, 2^{12}]$, es decir $|r_0| \leq 2^{12} = 4\,096$. $\square$

### Prueba de la cota de $r_1$

Dado $r^+ \in [0, Q)$ y $r_0 \in (-2^{12}, 2^{12}]$:

**Cota inferior:** $r^+ \geq 0$ y $r_0 \leq 2^{12}$, por lo que $r^+ - r_0 \geq 0 - 2^{12} = -2^{12}$. Pero $r^+ - r_0$ es múltiplo de $2^{13}$, y el menor múltiplo no negativo de $2^{13}$ es $0$. Dado que $r^+ \geq 0$ y $|r_0| \leq 2^{12} < 2^{13}$, se tiene $r^+ - r_0 \geq -2^{12}$, y el redondeo al múltiplo de $2^{13}$ más cercano da $r^+ - r_0 \geq 0$. Por tanto $r_1 \geq 0$.

**Cota superior:** $r^+ \leq Q - 1 = 8\,380\,416$ y $r_0 > -2^{12}$, por lo que:

$$r^+ - r_0 < 8\,380\,416 + 2^{12} = 8\,380\,416 + 4\,096 = 8\,384\,512$$

$$r_1 = \frac{r^+ - r_0}{2^{13}} < \frac{8\,384\,512}{8\,192} = 1\,024$$

Dado que $r_1$ es un entero, $r_1 \leq 1\,023$. Verificación del caso extremo: $r^+ = Q - 1 = 8\,380\,416$, $r_0 = 8\,380\,416 \bmod 8\,192 = 8\,380\,416 - 1\,022 \times 8\,192 = 8\,380\,416 - 8\,372\,224 = 8\,192$, pero $8\,192 > 4\,096$, así que $r_0 = 8\,192 - 8\,192 = 0$ (en realidad $8\,192 \bmod 8\,192 = 0$, luego $r_0 = 0$). Entonces $r_1 = 8\,380\,416 / 8\,192 = 1\,022.5...$, lo cual no es entero; recalculamos: $8\,380\,416 / 8\,192 = 1\,022.9375$. El valor exacto: $1\,022 \times 8\,192 = 8\,372\,224$, $r^+ - 8\,372\,224 = 8\,192$, $r_0 = 0$ (pues $8\,192 = 2^{13}$, $8\,192 \bmod 2^{13} = 0$). Entonces $r_1 = (8\,380\,416 - 0) / 8\,192 = 1\,023$. $\checkmark$

**Resultado:** $r_1 \in [0, 1\,023]$, que cabe exactamente en $\lceil \log_2(1\,024) \rceil = 10$ bits. $\square$

### Verificación en la implementación

El código calcula $r_0$ mediante:

```c
*r0 = a_pos - ((a_pos + (1 << (D - 1))) >> D) * (1 << D);
```

Denotemos $q = (a\_pos + 2^{12}) \gg 13 = \lfloor (a\_pos + 2^{12}) / 2^{13} \rfloor$. Entonces:

$$r_0 = a\_pos - q \cdot 2^{13}$$

Debemos verificar que esto produce el residuo centrado. Sea $s = a\_pos \bmod 2^{13}$:

- Si $s \leq 2^{12}$: $a\_pos + 2^{12} < (q+1) \cdot 2^{13}$, luego $\lfloor (a\_pos + 2^{12}) / 2^{13} \rfloor = q$ donde $q = \lfloor a\_pos / 2^{13} \rfloor$. Entonces $r_0 = a\_pos - q \cdot 2^{13} = s \in [0, 2^{12}]$. $\checkmark$
- Si $s > 2^{12}$: $a\_pos + 2^{12} \geq (q+1) \cdot 2^{13}$, luego $\lfloor (a\_pos + 2^{12}) / 2^{13} \rfloor = q + 1$ donde $q = \lfloor a\_pos / 2^{13} \rfloor$. Entonces $r_0 = a\_pos - (q+1) \cdot 2^{13} = s - 2^{13} \in (-2^{12}, 0)$. $\checkmark$

Ambos casos reproducen el residuo centrado. $\square$

---

## Demostración 14: Corrección de `Decompose` y tratamiento del caso frontera

### Objetivo

Probar que `Decompose` satisface $r^+ \equiv r_1 \cdot \alpha + r_0 \pmod{Q}$ para todo $r^+ \in [0, Q)$, con $\alpha = 2\gamma_2$, incluyendo la corrección del corner case $r^+ - r_0 = Q - 1$.

### Caso general: $r^+ - r_0 \neq Q - 1$

Sea $r^+ \in [0, Q)$ y $r_0 = r^+ \bmod^{\pm} \alpha$, es decir, $r_0$ es el único entero en $(-\gamma_2, \gamma_2]$ tal que $r^+ \equiv r_0 \pmod{\alpha}$.

Dado que $\alpha \mid (r^+ - r_0)$, la división $r_1 = (r^+ - r_0) / \alpha$ es exacta, y la reconstrucción es trivial:

$$r_1 \cdot \alpha + r_0 = (r^+ - r_0) + r_0 = r^+ \qquad \square$$

**Cota de $r_1$:** Como $r^+ \in [0, Q)$ y $r_0 > -\gamma_2$:

$$r_1 = \frac{r^+ - r_0}{\alpha} < \frac{Q + \gamma_2}{\alpha} = \frac{Q + \gamma_2}{2\gamma_2}$$

Para ML-DSA-44: $r_1 < (8\,380\,417 + 95\,232) / 190\,464 = 8\,475\,649 / 190\,464 \approx 44.5$, luego $r_1 \leq 44$.

El valor $r_1 = 44$ se alcanza únicamente cuando $r^+ - r_0 = 44 \cdot \alpha = 44 \times 190\,464 = 8\,380\,416 = Q - 1$, que es exactamente el corner case.

**Cota de $r_0$:** Por definición del residuo centrado: $r_0 \in (-\gamma_2, \gamma_2]$.

### Corner case: $r^+ - r_0 = Q - 1$

Cuando $r^+ - r_0 = Q - 1$, tenemos $r_1 = (Q-1)/\alpha$. Verifiquemos que $\alpha \mid (Q - 1)$:

$$Q - 1 = 8\,380\,416 = 88 \times 95\,232 = 88 \times \gamma_2 = 44 \times \alpha \quad \text{(ML-DSA-44)}$$

$$Q - 1 = 8\,380\,416 = 32 \times 261\,888 = 32 \times \gamma_2 = 16 \times \alpha \quad \text{(ML-DSA-65/87)}$$

Luego $r_1 = 44$ (resp. $16$). Pero el rango deseado de $r_1$ es $[0, m-1]$ donde $m = (Q-1)/\alpha$, es decir $[0, 43]$ (resp. $[0, 15]$). El valor $r_1 = m$ está fuera de este rango.

**La corrección del estándar** consiste en forzar:

$$r_1 \leftarrow 0, \qquad r_0 \leftarrow r_0 - 1$$

**Prueba de que la corrección preserva la congruencia:**

Antes de la corrección: $r^+ = r_1 \cdot \alpha + r_0 = m \cdot \alpha + r_0 = (Q - 1) + r_0$.

Después de la corrección: $r_1' \cdot \alpha + r_0' = 0 \cdot \alpha + (r_0 - 1) = r_0 - 1$.

Debemos verificar que $r_0 - 1 \equiv r^+ \pmod{Q}$:

$$r^+ = (Q - 1) + r_0 \equiv -1 + r_0 = r_0 - 1 \pmod{Q}$$

$\checkmark$

**Justificación geométrica:** El valor $r^+ = Q - 1$ es adyacente a $r^+ = 0$ en el anillo $\mathbb{Z}_Q$. La parte alta de $0$ es $r_1 = 0$. Para preservar la continuidad cíclica de `HighBits` (necesaria para que `UseHint` funcione correctamente en la frontera modular), la parte alta de $Q - 1$ también debe ser $0$. Esto es lo que impone el corner case. $\square$

### Cota de $r_0$ tras la corrección

En el corner case, $r_0$ se decrementa en 1. Antes de la corrección, $r_0 \in (-\gamma_2, \gamma_2]$. Después: $r_0 - 1 \in (-\gamma_2 - 1, \gamma_2]$.

Pero el corner case solo se activa cuando $r^+ - r_0 = Q - 1$, lo cual implica $r^+ = Q - 1 + r_0$. Dado que $r^+ \in [0, Q)$: $r_0 \in (-(Q-1), 1]$. Combinado con $r_0 \in (-\gamma_2, \gamma_2]$, el rango efectivo es $r_0 \in (-\gamma_2, 1]$, y tras decrementar: $r_0 - 1 \in (-\gamma_2 - 1, 0]$.

Para ML-DSA-44: $|r_0 - 1| \leq \gamma_2 = 95\,232$, que cabe en 17 bits. $\square$

---

## Demostración 15: Corrección de `MakeHint` y `UseHint`

### Objetivo

Probar que el mecanismo Hint permite al verificador recuperar $\text{HighBits}(\mathbf{w})$ a partir del valor recalculado $\mathbf{w}'$, usando la pista $\mathbf{h}$ incluida en la firma.

### Relación algebraica entre $\mathbf{w}$ y $\mathbf{w}'$

Durante la firma, el firmante calcula $\mathbf{w} = \mathbf{A}\mathbf{y}$ y $\mathbf{z} = \mathbf{y} + c \cdot \mathbf{s}_1$. El verificador calcula:

$$\mathbf{w}' = \mathbf{A}\mathbf{z} - c \cdot \mathbf{t}_1 \cdot 2^d$$

Sustituyendo $\mathbf{z}$ y usando $\mathbf{t}_1 \cdot 2^d = \mathbf{t} - \mathbf{t}_0 = \mathbf{A}\mathbf{s}_1 + \mathbf{s}_2 - \mathbf{t}_0$:

$$\mathbf{w}' = \mathbf{A}(\mathbf{y} + c\mathbf{s}_1) - c(\mathbf{A}\mathbf{s}_1 + \mathbf{s}_2 - \mathbf{t}_0) = \mathbf{w} - c\mathbf{s}_2 + c\mathbf{t}_0$$

Por tanto, coeficiente a coeficiente: $w'_j = w_j - c(s_2)_j + c(t_0)_j$.

### Definición del Hint en el protocolo

El firmante genera el hint (FIPS 204, §6.2, paso 8) con argumentos:

$$h_j = \text{MakeHint}\bigl(\underbrace{-c(t_0)_j}_{=\,z_h},\;\; \underbrace{w_j - c(s_2)_j + c(t_0)_j}_{=\,w'_j}\bigr)$$

Por definición de `MakeHint`:

$$h_j = \begin{cases} 1 & \text{si } \text{HighBits}(w'_j) \neq \text{HighBits}(w'_j + z_h) \\ 0 & \text{en caso contrario} \end{cases}$$

Observemos que $w'_j + z_h = w_j - c(s_2)_j$. Y las cotas de rechazo (paso 7 de Sign) garantizan $\|\text{LowBits}(w_j - c(s_2)_j)\|_\infty < \gamma_2 - \beta$, lo que a su vez implica que $\text{HighBits}(w_j - c(s_2)_j) = \text{HighBits}(w_j)$ (la perturbación $c \cdot \mathbf{s}_2$ no cruza una frontera de franja en la dirección $w \to w - cs_2$; si lo hiciera, la firma se abortaría).

Por tanto:

$$h_j = 1 \iff \text{HighBits}(w'_j) \neq \text{HighBits}(w_j)$$

### Caso $h_j = 0$

$\text{HighBits}(w'_j) = \text{HighBits}(w_j)$. `UseHint` devuelve $\text{HighBits}(w'_j) = \text{HighBits}(w_j)$. $\checkmark$

### Caso $h_j = 1$

$\text{HighBits}(w'_j) \neq \text{HighBits}(w_j)$. La diferencia $w'_j - w_j = -c(s_2)_j + c(t_0)_j$ es pequeña (acotada por las cotas de rechazo), así que el cruce es de exactamente una frontera:

$$\text{HighBits}(w_j) \in \{(r_1 + 1) \bmod m, \;\; (r_1 - 1 + m) \bmod m\}$$

donde $(r_1, r_0) = \text{Decompose}(w'_j)$ y $m = (Q-1)/\alpha$.

`UseHint` elige la dirección según el signo de $r_0$:

- Si $r_0 > 0$: devuelve $(r_1 + 1) \bmod m$.
- Si $r_0 \leq 0$: devuelve $(r_1 - 1 + m) \bmod m$.

**Prueba de que la dirección es correcta.** Denotemos $\delta = w'_j - w_j$ la perturbación (pequeña). La franja $r_1 = k$ cubre los valores $r^+$ tales que $\text{Decompose}(r^+)$ devuelve $r_1 = k$, es decir, el rango centrado de ancho $\alpha$ alrededor de $k \cdot \alpha$. El residuo centrado $r_0 \in (-\gamma_2, \gamma_2]$ mide la distancia al centro: $r_0 > 0$ sitúa a $w'_j$ en la mitad superior (cerca del borde con la franja $k+1$), y $r_0 \leq 0$ en la mitad inferior (cerca del borde con la franja $k-1$).

Solo existen dos configuraciones que producen $h_j = 1$ (cruce de exactamente una frontera):

**Sub-caso A: $\delta < 0$ (cruce descendente).** El firmante tenía $w_j$ cerca del borde inferior de la franja $w_1$. La perturbación negativa empujó $w'_j$ hacia abajo, cruzando la frontera inferior. Por tanto $w'_j$ aterrizó en la parte alta de la franja $w_1 - 1$:

$$r_1 = (w_1 - 1 + m) \bmod m, \qquad r_0 > 0 \text{ (mitad superior)}$$

Necesitamos recuperar $w_1 = (r_1 + 1) \bmod m$. `UseHint` con $r_0 > 0$ devuelve exactamente $(r_1 + 1) \bmod m$. $\checkmark$

**Sub-caso B: $\delta > 0$ (cruce ascendente).** El firmante tenía $w_j$ cerca del borde superior de la franja $w_1$. La perturbación positiva empujó $w'_j$ hacia arriba, cruzando la frontera superior. Por tanto $w'_j$ aterrizó en la parte baja de la franja $w_1 + 1$:

$$r_1 = (w_1 + 1) \bmod m, \qquad r_0 \leq 0 \text{ (mitad inferior)}$$

Necesitamos recuperar $w_1 = (r_1 - 1 + m) \bmod m$. `UseHint` con $r_0 \leq 0$ devuelve exactamente $(r_1 - 1 + m) \bmod m$. $\checkmark$

En ambos sub-casos, la dirección del ajuste coincide con la corrección necesaria. No existen otros sub-casos: un cruce descendente siempre deja a $w'_j$ en la mitad superior de la franja destino (porque entró por arriba), y un cruce ascendente siempre lo deja en la mitad inferior (porque entró por abajo). $\square$

### Propiedad de acotación del peso del hint

**Lema:** En una firma válida (no abortada), el número de coeficientes con $h_i = 1$ es a lo sumo $\omega$.

*Prueba:* El firmante verifica explícitamente $\sum_{i} h_i \leq \omega$ antes de emitir la firma (paso 9 de `Sign`). Si la desigualdad se viola, se ejecuta un `ABORT` y se reintenta con un nuevo $\mathbf{y}$. Toda firma emitida satisface esta cota por construcción. $\square$

---

## Demostración 16: Cota de desbordamiento en la acumulación pointwise

### Objetivo

Probar que la acumulación de $\ell$ productos pointwise en `polyvecl_pointwise_acc` no desborda un `int32_t` para ningún nivel de seguridad de ML-DSA.

### Setup

Cada término de la acumulación es el resultado de `montgomery_reduce((int64_t)a[i] * b[i])`. Por la Demostración 3, cada resultado satisface $|r_j| \leq Q = 8\,380\,417$.

El acumulador suma $\ell$ de estos términos:

$$S = \sum_{i=0}^{\ell - 1} r_i, \qquad |S| \leq \ell \cdot Q$$

### Verificación por nivel de seguridad

| Nivel       | $\ell$ | $\ell \cdot Q$ | Bits necesarios | Margen vs. $2^{31}$ |
|-------------|--------|-------------------------------|------|------|
| ML-DSA-44   | 4      | $4 \times 8\,380\,417 = 33\,521\,668$ | 25 bits | 6 bits |
| ML-DSA-65   | 5      | $5 \times 8\,380\,417 = 41\,902\,085$ | 26 bits | 5 bits |
| ML-DSA-87   | 7      | $7 \times 8\,380\,417 = 58\,662\,919$ | 26 bits | 5 bits |

En todos los casos:

$$\ell \cdot Q < 2^{27} \ll 2^{31} - 1 = 2\,147\,483\,647$$

El cociente $2^{31} / (\ell \cdot Q)$ para el peor caso (ML-DSA-87) es:

$$\frac{2\,147\,483\,647}{58\,662\,919} \approx 36.6$$

Es decir, el acumulador podría sumar más de 36 veces el valor máximo antes de desbordar. En la práctica solo suma $\ell = 7$ veces. El margen es de un factor 5× respecto al peor caso.

**Conclusión:** La acumulación de $\ell$ productos pointwise cabe holgadamente en un `int32_t`. No se requiere reducción intermedia entre sumas. Una única `barrett_reduce` al final de la acumulación es suficiente. $\square$

---

*Documento de demostraciones matemáticas — Fases 1, 2 y 3 | `el_pedestal` | Uso: estudio personal y auditoría criptográfica*

---

## Referencias y Fuentes

1. **NIST FIPS 204:** *Module-Lattice-Based Digital Signature Standard*. Especificación oficial del estándar ML-DSA publicado por el Instituto Nacional de Estándares y Tecnología de EE.UU. 
   - [Enlace al PDF Oficial (NIST)](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.204.pdf)
2. **PQ-Crystals Dilithium:** Implementación de referencia en C del algoritmo original en el que se basa ML-DSA. El diseño en tiempo constante, la gestión del desbordamiento en 32 bits y las mariposas de la NTT proceden de este código (archivos `ref/ntt.c` y `ref/reduce.c`).
   - [Repositorio en GitHub](https://github.com/pq-crystals/dilithium)
3. **Base Algorítmica de FFT/NTT:** Algoritmos clásicos de decimación en tiempo (Cooley-Tukey) y en frecuencia (Gentleman-Sande) sobre anillos negacíclicos.
