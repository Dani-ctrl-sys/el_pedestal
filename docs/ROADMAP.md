# ROADMAP — `el_pedestal`

**Documento descriptivo y mapa arquitectónico del proyecto.**  
*Estándar:* FIPS 204 (ML-DSA) | *Target:* C99 bare-metal, 32 bits.

---

## 1. ¿Qué estamos construyendo? (El problema base)

Nos enfrentamos al inminente avance de la computación cuántica. Si un ordenador cuántico con suficientes qubits estables se enciende mañana, toda la criptografía actual basada en RSA o curvas elípticas (ECDSA) caerá instantáneamente. Cualquier secreto bancario, militar o de identidad protegido hoy, quedaría al descubierto.

Para evitar esto, el gobierno de los Estados Unidos (NIST) ha publicado un nuevo estándar llamado **FIPS 204**, protagonizado por un algoritmo conocido como **ML-DSA** (antes CRYSTALS-Dilithium). Sus matemáticas no se basan en factorizar números gigantes, sino en esconder información en *retículas espaciales complejas (Lattices)* con ruido, un problema donde los ordenadores cuánticos no tienen ventajas mágicas.

El proyecto **`el_pedestal`** es una implementación artesanal de ML-DSA. Nuestro objetivo es que este algoritmo tan complejo pueda ejecutarse en procesadores diminutos (microcontroladores de 32-bits muy limitados), sin usar funciones modernas, sin reserva dinámica de memoria (nada de `malloc`), y con un código cristalino para ser auditable.

---

## 2. Restricciones de Ingeniería (Las reglas del juego)

Para poder compilarse en cualquier tostadora, robot o smartcard, todo el código debe seguir un conjunto de reglas draconianas:

1. **C99 Puro**: Compilación en cualquier arquitectura moderna o legacy.
2. **"Bare-metal"**: Nada de sistema operativo. Nada de Heap. Toda la memoria se aloja en Stack de forma determinista para que jamás se cuelgue por fallos de enrutamiento de RAM.
3. **Tiempo Constante (Constant-Time)**: Para que un cibercriminal no deduzca claves secretas midiendo los nanosegundos que tarda la CPU en la ejecución, queda estrictamente **prohibido** el uso de expresiones `if` y bucles cuyos límites dependan de datos secretos.
4. **Optimización Extrema**: ML-DSA requiere millones de operaciones matemáticas para firmar una sola vez. No podemos darnos el lujo de tirar ciclos de CPU a la basura.

---

## 3. Arquitectura del Algoritmo (Pieza a Pieza)

El desarrollo del algoritmo no se hace de golpe; se ensambla en capas como una pirámide, donde el bloque de arriba es inútil si los cimientos fallan. Hemos dividido la montaña en varios "temas" lógicos:

### Bloque A: Primitivas Hash (El motor alfanumérico)
En criptografía moderna casi todo arranca con entropía y caos. Necesitamos la función estándar **Keccak (SHAKE-128 / SHAKE-256)** que toma datos de cualquier tamaño y los tritura para escupir:
*   Burbujas de números pseudo-aleatorios (para generar secretos iniciales fuertes).
*   *Hashes* indescifrables (para generar el "sello" único del mensaje que queremos firmar).

### Bloque B: La Aritmética Base (Matemáticas acotadas)
Todo el sistema trabaja en un anillo de números limitados entre el 0 y el 8.380.416 (una constante `Q` obligatoria de NIST). Nada puede salirse de ahí. Toda multiplicación requiere una división (módulo), y las CPUs odian dividir. Inventamos maneras geniales de *dividir con desplazamientos binarios*:
*   **Montgomery**: Especialista en reducir multiplicaciones letales.
*   **Barrett**: Útil para bajar acumuladores grandes de regreso al tamaño acordado.

### Bloque C: Acelerador Matemático (La NTT)
Una "firma" es un polinomio larguísimo, y para validar una hay que cruzar, cruzar y multiplicar miles de números entre sí. El cruce (convolución) tardaría horas ejecutándolo de forma natural, por lo que adaptamos una función de ondas llamada Number Theoretic Transform (**NTT**). Transforma los polinomios en un estado cuántatitativo donde la multiplicación es lineal y barata en ciclos de reloj.

### Bloque D: Vectores y Bloques (La carga pesada)
Hacemos matrices y columnas utilizando los pequeños anillos de polinomios que hemos perfeccionado usando NTT. Las llaves privadas ($\mathbf{s}_1, \mathbf{s}_2$) son listas ocultas con números pequeños. Las llaves públicas ($\mathbf{t}$) son las variaciones repletas de ruido artificial para emborronar el rastro matemático.

### Bloque E: Compresión y "Aborts" (Eficiencia de red)
Si enviáramos la firma ML-DSA completa, ocuparía demasiado (Megabytes para miles de firmas). 
*   Para ahorrar bytes, usamos algoritmos de **Compresión** empujando valores bajos (recortes por *Power2round*, *HighBits* y *LowBits*).
*   **Vector de Pistas (Hints)**: Como emborronamos la firma para seguridad pero recortamos bytes, perdemos sincronización de datos. Hacemos unas "pistas de error" booleanas de 1bit para que el que valida la firma lo compense de forma correcta.

### Bloque F: El Ensamblador Criptográfico
Uniendo todas y cada una de las piezas creamos el triforce clásico:
1.  **KeyGen**: Escupir una Llave Pública compartida y Llave Privada segura.
2.  **Sign**: Generar el rechazo seguro donde metemos el fichero a proteger y comprobamos mediante la técnica *Fiat-Shamir con Abortos* que el sello no revela nada de la Llave Privada. (Si lo hace, la función *aborta* y lo intenta de cero internamente).
3.  **Verify**: El algoritmo donde pasamos la firma que dice el otro lado, pasamos los datos, y este reconstruye las matemáticas hasta validar que de la clave pública expuesta se pueda sacar la traza fidedigna de la firma privada.

---

## 4. Hoja de Ruta Actual (Roadmap de Avance)

Como en todo proceso industrial, debemos ir programando un bloque, validándolo, y usándolo para el siguiente. A continuación se desglosa el plan de ataque sobre lo que hemos progresado y qué falta terminar para llegar a la API completa FIPS 204:

### ✅ FASES COMPLETADAS (Mantenimiento / Auditoría)
- **Aritmética Inferior** (Módulo `arithmetic.c`): Barrett, Montgomery, condicionales sin Branching. **OK**.
- **Motor NTT** (Módulo `ntt.c`): Generador Zetas óptimo y NTT en capas validada bajo test in-built. **OK**.
- **Estructuras de Capacidad**: Structs estandarizados para M-DSA 44, 65, 87 en RAM estática. **OK**.

### 🏗️ FASE EN DESARROLLO ACTUAL (El Avance Principal)
**[COMPRESIÓN] y Matrices Core**
Estamos lidiando en la fase de recortar eficientemente las claves a través de `Decompose` / `Power2Round` y la lógica precisa del **Vector de Hinting**. 

> [!TIP]
> **Punto donde retomar:**
> El problema inminente actual que debemos afinar es la implementación de la función "MakeHint" y "UseHint", garantizando que todo trabaje de bajo nivel sin reservar colas dinámicas, y respetando la variable limitante matemática global $\omega$ (el número máximo de 1's que NIST permite tener como errores dentro del Hint de un solo uso).

### 🚀 FASES FUTURAS (Next Actions)
1.  **Hashing Base**: Implementar la Esponja primitiva Keccak/SHAKE.
2.  **Compilación Matrices Generadoras**: Implementar la generación determinista de la matriz matriz $\mathbf{A}$ usando el SHAKE-128 con la semilla estática "rho".
3.  **Ensamblado Final KeyGen**: Mezclar los polinomios con la aleatoridad entrópica y publicar llaves.
4.  **Bucle de Firma F-S**: Entrelazar la creación del reto criptográfico "c", validarlo. Prototipar el bucle del *Rejection Sampling* que protege contra fugas probabilísticas.
5.  **Validación Verify y End-to-end**: El test final pasando los test-vectors del NIST para certificar la librería como *Compliance Ready*.
