# Conceptos Básicos y Glosario — `el_pedestal`

> **Propósito:** Esta guía está escrita para principiantes absolutos. Si los términos matemáticos te intimidan, aquí explicaremos con metáforas y analogías cotidianas los pilares sobre los que se sustenta toda la arquitectura de ML-DSA (el algoritmo en el que se basa este proyecto), seguidos de su definición matemática rigurosa para ir calentando motores.
>
> 💡 *Sugerencia: Lee esta página antes de adentrarte en la `STUDY_GUIDE.md` o en las demostraciones matemáticas.*
>
> - Siguiente paso → [`STUDY_GUIDE.md`](STUDY_GUIDE.md) (arquitectura e intuición)
> - Demostraciones formales → [`MATH_PROOFS.md`](MATH_PROOFS.md)
> - Contratos de interfaz → [`DESIGN.md`](../DESIGN.md)

---

## 1. El Módulo ($Q$) y la Aritmética Modular

**La Analogía:**
En nuestro día a día, usamos una recta numérica infinita. Si sumas números, te vas infinitamente hacia la derecha.

En **aritmética modular**, los números no están en una recta, sino en un **reloj**. En ML-DSA, nuestro "reloj" tiene exactamente $Q = 8\,380\,417$ horas (posiciones, numeradas del 0 al 8.380.416).
- Si a $8\,380\,416$ le sumas $1$, no pasas a $8\,380\,417$, sino que das la vuelta al reloj y vuelves a empezar en **$0$**.
- Si a $0$ le restas $1$, no vas a $-1$, sino que retrocedes a las últimas "horas" del reloj, aterrizando en **$8\,380\,416$**.

A los ordenadores no se les da bien manejar números infinitamente grandes. Restringiendo todo nuestro universo a un reloj finito (el módulo $Q$), garantizamos que ningún número ocupará más megabytes de los debidos. 

**La Definición Matemática:**
> Todo cálculo se realiza en el **Anillo Residual $\mathbb{Z}_Q$**, donde $Q = 8\,380\,417$ es un número primo. 
> Matemáticamente, decimos que $a \equiv b \pmod{Q}$ si la diferencia $(a - b)$ es un múltiplo exacto de $Q$. Todos los coeficientes del esquema pertenecen al conjunto finito $\{0, 1, 2, \dots, Q-1\}$. 

---

## 2. Los Polinomios (Vectores)

**La Analogía:**
A nivel matemático, verás que hablamos de "anillos polinómicos", un término que asusta. A nivel de programación, **un polinomio de ML-DSA es simplemente una matriz o lista (Array) de exactamente 256 números enteros**. Cada uno de estos enteros es una "hora" en el reloj $Q$ que hemos mencionado antes.

Cuando decimos que sumamos dos polinomios, lo único que estamos haciendo internamente es coger el elemento 0 del primer array y sumarlo con el 0 del segundo, y repetir esto 256 veces. 

En lugar de proteger secretos basados en números enteros gigantes (como se hacía en RSA), nosotros usamos configuraciones geométricas multidimensionales de estos arrays (vectores y retículos).

**La Definición Matemática:**
> Los vectores operan en el **Anillo Polinómico Cociente $R_q = \mathbb{Z}_Q[X]/(X^{256} + 1)$**.
> Esto significa que un polinomio $a(X) = a_0 + a_1 X + \dots + a_{255} X^{255}$ tiene $N=256$ coeficientes. La condición $(X^{256} + 1)$ indica que durante cualquier multiplicación, si aparece un término $X^{256}$, este se sustituye automáticamente por $-1$ (propiedad de "reducción negacíclica").

---

## 3. La NTT (Transformada Teórica de Números)

**La Analogía:**
Multiplicar dos arrays entre sí (la "convolución polinómica") es una pesadilla de rendimiento porque cada número de un array tiene que multiplicarse por todos y cada uno de los 256 números del otro array (65.536 multiplicaciones). 

Aquí es donde entra la **NTT (Number Theoretic Transform)**.
Imagina que quieres mezclar dos latas de pintura de distinto color (esa mezcla es la multiplicación lenta). La NTT es un agujero de gusano espacial que te teletransporta a un universo paralelo (el **Dominio de Evaluación**). 
- Si teletransportamos ambas latas a este universo, mezclarlas allí es extremadamente fácil y ultrarrápido (solo requiere 256 multiplicaciones).
- Tras mezclarlas, usamos la **INTT** (NTT Inversa) para teletransportar el resultado mezclado de vuelta a nuestro universo normal.

**La Definición Matemática:**
> La **NTT** es el análogo en cuerpos finitos de la Transformada Rápida de Fourier (FFT). Evalúa el polinomio en las 256 raíces de la unidad de la ecuación $X^{256}+1=0$. Como el anillo soporta una raíz primitiva 512-ésima de la unidad $\zeta = 1753 \pmod{Q}$, la NTT convierte la compleja convolución de polinomios $\mathcal{O}(N^2)$ en una multiplicación componente a componente (*pointwise*) en tiempo logarítmico $\mathcal{O}(N \log N)$.

---

## 4. Diseño Branchless (Tiempo Constante)

**La Analogía:**
Seguramente has usado la sentencia `if / else` al programar. En criptografía, eso es abrir la puerta a un ladrón.

Imagina un ladrón de cajas fuertes con un fonendoscopio escuchando el clic de tu cerradura. Ese es un **ataque de canal lateral (*Side-channel attack*, o *Timing attack*)**. Si un ordenador ejecuta un `if (contraseña == 1) HazA(); else HazB();`, un hacker malicioso sabe —con solo medir los microsegundos que tarda la CPU— por cuál rama se fue el código y deducirá tu contraseña.

Para evitar esto, programamos **Branchless (Sin Ramificaciones)**. Nunca saltamos instrucciones. Si no debes hacer A, el código "hace" A de todos modos pero multiplica el resultado de incógnito por $0$ para borrarlo. Todo dura exactamente los mismos microsegundos, aislando acústicamente la caja fuerte.

**La Definición Matemática / Arquitectónica:**
> Las implementaciones de **Constant-Time** afirman que el flujo de ejecución dependiente (Control Flow Graph) y los patrones de acceso temporales a memoria deben ser totalmente independientes de la variable de estado secreta. Se reemplazan las bifurcaciones y los predicados de control por multiplexores aritméticos de enmascaramiento booleano puro (ej. derivando máscaras de bits vectoriales de todo unos `0xFFFFFFFF` o todo ceros `0x00000000` basándose en la comprobación del bit lógico de signo).

---

## 5. El "Complemento a Dos" (Representación de Números)

**La Analogía:**
Cuando la CPU procesa el número negativo `-1`, lo representa encendiendo todos sus interruptores a la vez. Así `-1` en hexadecimal se escribe en 32 bits como `0xFFFFFFFF`. Y el cero se escribe `0x00000000`.

Saber esto "hackea" el comportamiento del ordenador: nos permite convertir un número negativo en una máscara perfecta para borrar datos de un plumazo usando lógica de compuertas (AND/OR), y nos ahorra tener que preguntar al ordenador "¿eres negativo?" usando un peligroso salto de `if`.

**La Definición Matemática:**
> El **Complemento a Dos** es la representación binaria canónica de números enteros con signo. Un entero $x \in [-2^{31}, 2^{31}-1]$ se evalúa algebraicamente (sobre 32 bits) como $-b_{31} \cdot 2^{31} + \sum_{i=0}^{30} b_i 2^i$. El bit más significativo ($b_{31}$) actúa como bit formal de signo, permitiendo que las operaciones nativas de suma y resta en hardware ignoren el signo operando puramente mediante aritmética modular sobre el anillo finito de bits $\mathbb{Z}/2^{32}\mathbb{Z}$.

---

# Fase 3 — Vectores, Compresión y Hints

---

## 6. Vectores y Matrices de Polinomios

**La Analogía:**
Ya sabemos que un polinomio es un array de 256 números. Ahora imagina que metes 4 de esos arrays dentro de una caja. Esa caja es un **vector de polinomios**. Y si apilas varias cajas en una estantería formando una cuadrícula, tienes una **matriz de polinomios**.

En ML-DSA, la clave pública es básicamente una estantería ($\mathbf{A}$) de $K \times L$ cajones, donde cada cajón contiene un polinomio de 256 números. La clave secreta son dos cajas ($\mathbf{s}_1$, $\mathbf{s}_2$) llenas de polinomios con coeficientes muy pequeños (secretos). El acto de multiplicar la estantería por la caja secreta ($\mathbf{t} = \mathbf{A} \cdot \mathbf{s}_1 + \mathbf{s}_2$) es lo que genera la clave pública.

**La Definición Matemática:**
> Un **vector polinómico** $\mathbf{v} \in R_q^k$ es una tupla ordenada de $k$ elementos del anillo $R_q = \mathbb{Z}_Q[X]/(X^{256}+1)$. Una **matriz** $\mathbf{A} \in R_q^{k \times \ell}$ es una disposición rectangular de $k \cdot \ell$ polinomios. El producto matriz-vector $\mathbf{A} \cdot \mathbf{s}$ se calcula mediante productos internos: la fila $i$-ésima del resultado es $\sum_{j=0}^{\ell-1} \mathbf{A}_{i,j} \cdot \mathbf{s}_j$, donde cada multiplicación de polinomios se realiza eficientemente en el dominio NTT.

---

## 7. Compresión: Power2Round y Decompose

**La Analogía:**
Imagina que tienes una fotografía de 23 megapíxeles (un coeficiente de 23 bits), pero necesitas enviarla por una conexión lenta. La solución obvia es comprimir la foto: separar la información importante (las formas y los colores principales) de los detalles finos (el grano, el "ruido"). Envías solo lo importante y guardas el detalle en privado.

Eso es exactamente lo que hacen **Power2Round** y **Decompose**: cortan cada número de 23 bits en dos trozos.

- **Power2Round** usa unas tijeras muy simples: corta por el bit 13, como quien arranca las últimas 13 páginas de un libro. Es rápido y barato (un simple desplazamiento de bits `>> 13`). Se usa una sola vez durante la generación de claves.
- **Decompose** usa un bisturí más sofisticado: divide por un número $\alpha$ que **no** es potencia de 2 (por ejemplo 190.464), lo que requiere una división "de verdad". Se usa durante cada firma y cada verificación. Es más costoso, pero produce franjas uniformes que encajan mejor con la geometría del esquema de rechazo.

**La Definición Matemática:**
> **Power2Round** descompone un coeficiente $r \in \mathbb{Z}_Q$ como $r = r_1 \cdot 2^d + r_0$, donde $d = 13$, $r_1 = \lfloor (r + 2^{d-1}) / 2^d \rfloor$ es la **parte alta** (10 bits, se publica), y $r_0 = r - r_1 \cdot 2^d$ es la **parte baja** centrada en $(-2^{d-1}, 2^{d-1}]$ (se guarda en privado).
>
> **Decompose** descompone $r$ como $r = r_1 \cdot \alpha + r_0$, donde $\alpha = 2\gamma_2$. El cociente $r_1$ se llama **HighBits** y el residuo $r_0$ se llama **LowBits**. El caso especial $r = Q-1$ se trata forzando $r_1 = 0$ y $r_0 = r - 1$ para que $r_1$ nunca exceda el rango $[0, (Q-1)/\alpha)$.

---

## 8. El Mecanismo de Hints (Pistas de Corrección)

**La Analogía:**
Imagina que dos personas están midiendo la misma pared con reglas diferentes. El firmante mide con una regla de precisión milimétrica y obtiene "3 metros y 47 centímetros". El verificador mide con una regla más tosca y obtiene "3 metros y 52 centímetros". Las partes altas coinciden ("3 metros"), pero los centímetros difieren un poco. El problema es: ¿y si la diferencia de centímetros hace que uno redondee a "3 metros" y el otro a "4 metros"? Eso sería un desastre criptográfico: la firma se rechazaría aunque fuera auténtica.

Para solucionar esto, el firmante incluye una **pista (hint)** en la firma: un simple bit que dice "ojo, en esta posición yo obtuve un redondeo diferente al que tú vas a obtener, así que súmale uno a tu resultado". Con esa pista de un solo bit, el verificador puede corregir su medición y ambos acaban con la misma parte alta, sin que el verificador necesite conocer la clave secreta.

**La Definición Matemática:**
> Sea $r = r_1 \cdot \alpha + r_0$ la descomposición del valor del firmante, y sea $r' = r + z$ el valor perturbado que calcula el verificador (donde $z$ es parte de la firma). Definimos:
>
> $$\text{MakeHint}(r_0, r') = \begin{cases} 1 & \text{si } \text{HighBits}(r) \neq \text{HighBits}(r') \\ 0 & \text{si } \text{HighBits}(r) = \text{HighBits}(r') \end{cases}$$
>
> El hint $h \in \{0, 1\}^N$ es un vector de bits booleanos. La función **UseHint** permite al verificador recuperar $\text{HighBits}(r)$ a partir de $r'$ y $h$ sumando o restando 1 al cociente alto cuando $h_i = 1$, sin conocer $r_0$ ni la clave secreta.
> La firma impone una cota máxima $\omega$ al número de hints activos ($\sum h_i \leq \omega$), limitando así el tamaño de la firma y evitando que un adversario abuse del mecanismo.
