# Conceptos Básicos y Glosario — `el_pedestal`

> **Propósito:** Esta guía está escrita para principiantes absolutos. Si los términos matemáticos te intimidan, aquí explicaremos con metáforas y analogías cotidianas los pilares sobre los que se sustenta toda la arquitectura de ML-DSA (el algoritmo en el que se basa este proyecto).
>
> 💡 *Sugerencia: Lee esta página antes de adentrarte en la `STUDY_GUIDE.md` o en las demostraciones matemáticas.*

---

## 1. El Módulo ($Q$) y la Aritmética Modular

En nuestro día a día, usamos una recta numérica infinita. Si sumas números, te vas infinitamente hacia la derecha.

En **aritmética modular**, los números no están en una recta, sino en un **reloj**. En ML-DSA, nuestro "reloj" tiene exactamente $Q = 8\,380\,417$ horas (posiciones, numeradas del 0 al 8.380.416).
- Si a $8\,380\,416$ le sumas $1$, no pasas a $8\,380\,417$, sino que das la vuelta al reloj y vuelves a empezar en **$0$**.
- Si a $0$ le restas $1$, no vas a $-1$, sino que retrocedes a las últimas "horas" del reloj, aterrizando en **$8\,380\,416$**.

**¿Por qué lo hacemos así?** 
A los ordenadores no se les da bien manejar números infinitamente grandes. Restringiendo todo nuestro universo a un reloj finito (el módulo $Q$), garantizamos que ningún número ocupará más megabytes de los debidos. Además, las matemáticas modulares actúan como una batidora gigante que mezcla los números para que sea imposible hacer ingeniería inversa (algo vital en criptografía).

---

## 2. Los Polinomios (Vectores)

A nivel matemático, verás que hablamos de "anillos polinómicos", un término que asusta.

A nivel de programación, **un polinomio de ML-DSA es simplemente una matriz o lista (Array) de exactamente 256 números enteros**. Cada uno de estos enteros es una "hora" en el reloj $Q$ que hemos mencionado antes.

Cuando decimos que sumamos dos polinomios, lo único que estamos haciendo internamente es lo siguiente:
- Cogemos el elemento 0 del primer array y lo sumamos con el elemento 0 del segundo array.
- Cogemos el elemento 1 y lo sumamos con el elemento 1... y así sucesivamente, con la condición de apuntar la respuesta en nuestro "reloj $Q$".

En lugar de proteger secretos basados en números enteros gigantes (como se hacía en RSA), nosotros usamos configuraciones geométricas de estos arrays en retículos multidimensionales.

---

## 3. La NTT (Transformada Teórica de Números)

Multiplicar dos arrays entre sí (hacer la "convolución polinómica") es una pesadilla de rendimiento porque cada número de un array tiene que multiplicarse por todos y cada uno de los 256 números del otro array (haciendo 65.536 multiplicaciones). Un dispositivo en red o un teléfono se ahogaría si tuviera que firmar mil contratos por segundo con ese cálculo.

Aquí es donde entra la **NTT (Number Theoretic Transform)**.

Imagina que quieres mezclar dos latas de pintura de distinto color (esa mezcla es la multiplicación lenta). La NTT es una operación mágica que te permite teletransportar tu problema a un universo paralelo (el **Dominio de Evaluación** o **Dominio NTT**). 

- Si teletransportamos ambos arrays a este "universo NTT", entonces mezclarlos es extremadamente fácil y ultrarrápido (solo requiere 256 de las operaciones directas, y fin).
- Una vez hemos mezclado los arrays en el otro universo, invocamos a la **INTT** (NTT Inversa) para teletransportar el resultado mezclado de vuelta a nuestro universo normal y tener las firmas digitales correctas.

---

## 4. Diseño Branchless (Tiempo Constante)

Seguramente has usado la sentencia `if / else` al programar. En criptografía, usar `if` **es extremadamente peligroso**.

Imagina un ladrón de cajas fuertes que pone su oído junto a la puerta de hierro, escuchando el clic de las ruedas de la cerradura. Al escuchar las ruedas, sabe cómo son los engranajes sin necesidad de ver la llave. Ese es el equivalente a un **ataque de canal lateral (*Side-channel attack*)**. Si un ordenador ejecuta un `if (contraseña_secreta == 1) HazA(); else HazB();`, un hacker malicioso sabe —con solo medir con un cronómetro los microsegundos que tarda en completarse la instrucción— por cuál rama se fue tu programa y, por consiguiente, puede averiguar tu contraseña secreta.

Para evitar esto, programamos de manera **Constant-Time (Tiempo Constante)** o **Branchless (Sin Ramificaciones)**. Nunca saltamos instrucciones en función del valor de los datos. Procesamos `HazA()` y procedemos a procesar `HazB()` sin bifurcaciones de código asumiendo el mismo gasto de reloj en la CPU, enmascarando mediante máscaras de bits o matemáticas la opción que no debíamos tomar, y dejando solo lo que sí era la respuesta final en los circuitos de la memoria. Todos los cálculos duran siempre y exactamente los mismos nanosegundos protegiendo su integridad.

---

## 5. El "Complemento a Dos" (Representación de Números)

Cuando la CPU procesa el número negativo `-1`, lo representa en el formato **complemento a dos** rellenándolo con el bit "1" encendido en todo su registro. Así `-1` en hexadecimal se escribe en 32 bits como `0xFFFFFFFF`. Y el cero se escribe `0x00000000`.

En este esquema no hay distinción de `signos(+ ó -)` almacenada en la CPU. Jugar con estos bloques es nuestra forma más barata y optimizada (¡Y segura para *tiempo constante*!) de aplicar máscaras de borrado y operaciones de suma.
