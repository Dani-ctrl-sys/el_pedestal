# 🏛️ El Pedestal: ML-DSA Bare-Metal

**El Pedestal** es una implementación educativa y de grado de ingeniería de **ML-DSA** (Module-Lattice-Based Digital Signature Standard, FIPS 204), diseñada específicamente para sistemas embebidos de 32 bits (C99).

Su enfoque principal no es solo proveer código funcional, sino **desmitificar la criptografía post-cuántica** mediante una batería de justificaciones matemáticas, arquitectura estrictamente *branchless* (tiempo constante) y analogías visuales enfocadas en el "por qué" de cada instrucción.

---

## 🗺️ Mapa de Lectura (Documentación)

Este proyecto cuenta con una documentación piramidal para poder ser abordado independientemente de tus conocimientos previos en anillos polinómicos o seguridad criptográfica:

1. 🟢 **Nivel Principiante:** [**Conceptos Básicos** (`docs/CONCEPTS.md`)](docs/CONCEPTS.md)
   Empieza aquí si la criptografía de retículos te suena a magia negra. Contiene analogías (relojes, agujeros de gusano de NTT y cajas fuertes) junto con el aterrizaje a sus definiciones puras.

2. 🟡 **Nivel Intermedio:** [**Guía de Estudio** (`docs/STUDY_GUIDE.md`)](docs/STUDY_GUIDE.md)
   El manual del ingeniero. Explica la intuición y arquitectura paso a paso: desde anular ramas condicionales para neutralizar ataques Side-Channel, hasta cómo funciona el escalado en el dominio de la Transformada Teórica de Números.

3. 🔴 **Nivel Avanzado (Experto):** [**Demostraciones Matemáticas** (`docs/MATH_PROOFS.md`)](docs/MATH_PROOFS.md)
   El abismo algebraico. Aquí se derivan rigurosamente por qué $Q=8\,380\,417$, los límites de desbordamiento de la Reducción de Barrett, y el levantamiento de Hensel (2-ádico) para la optimización de Montgomery. Cero abstracciones, 100% rigor analítico.

4. 🏗️ **Arquitectura de Software:** [**Manifiesto de Diseño** (`DESIGN.md`)](DESIGN.md)
   Contratos de interfaz, manejo de memoria, y toma de decisiones de la implementación final en código C, ideado puramente para ser independiente del SO.

---

## 🚀 Fases del Proyecto

El desarrollo y estudio de ML-DSA en *el_pedestal* sigue un ciclo orgánico y metódico:

- [x] **Fase 1:** Aritmética Modular (*Branchless, Montgomery, Barrett*).
- [x] **Fase 2:** La NTT (*Transformada de Fourier sobre dominios finitos, Cooley-Tukey y Gentleman-Sande*).
- [ ] **Fase 3:** Compresión y Contenedores — *documentación teórica completada; implementación en C pendiente.*
- [ ] **Fase 4:** Primitivas Criptográficas de Hashing (*SHAKE-128/256, Expansiones*).
- [ ] **Fase 5:** Funciones Superiores y Ensamblaje (*KeyGen, Sign, Verify*).

---

## 🛠️ Compilación y Pruebas

Al ser un proyecto totalmente portátil escrito en estricto C99 de 32 bits, no tiene dependencias dinámicas (*bare-metal*).

Puedes verificar la integridad criptográfica de la aritmética en el dominio de evaluación NTT mediante su `Makefile`:

```bash
make
./build/test_arith
```
*(Los tests validan la reducción analítica, el ciclo hermético de la NTT inversa y la gestión del desbordamiento en memoria sobre todos los anillos).*

---

> *"Para poder ver lejos, a veces no necesitas magia cuántica... solo construir el pedestal correcto."*
