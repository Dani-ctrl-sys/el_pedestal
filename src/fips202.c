#include "fips202.h"

/* 
 * =============================================================================
 * 1. ROTACIONES Y GESTIÓN DE MEMORIA SEGURA (Bare-metal endianness)
 * =============================================================================
 */

/* 
 * Macro ROTL64: Rota un número de 64 bits hacia la izquierda.
 * Es la base del movimiento de las matrices Keccak.
 */
#define ROTL64(x, c) (((x) << (c)) | ((x) >> (64 - (c))))

/* 
 * load64 y store64: 
 * Leen y escriben enteros de 64 bits de forma segura byte a byte. 
 * Si leyésemos directamente desde un puntero en un Cortex-M0 sin alinear, 
 * el procesador sufriría un "HardFault" y se colgaría.
 */
static uint64_t load64(const uint8_t x[8]) {
    uint64_t r = 0;
    for (int i = 0; i < 8; ++i) {
        r |= (uint64_t)x[i] << (8 * i);
    }
    return r;
}

static void store64(uint8_t x[8], uint64_t u) {
    for (int i = 0; i < 8; ++i) {
        x[i] = (uint8_t)(u >> (8 * i));
    }
}

/* 
 * =============================================================================
 * 2. COMBUSTIBLE MATEMÁTICO: LAS 24 CONSTANTES DE RONDA
 * =============================================================================
 * Keccak ejecuta 24 "vueltas" para destruir cualquier patrón. 
 * En cada vuelta, le inyecta uno de estos números mágicos asimétricos.
 */
static const uint64_t KeccakF_RoundConstants[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL,
    0x800000000000808aULL, 0x8000000080008000ULL,
    0x000000000000808bULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL,
    0x000000000000008aULL, 0x0000000000000088ULL,
    0x0000000080008009ULL, 0x000000008000000aULL,
    0x000000008000808bULL, 0x800000000000008bULL,
    0x8000000000008089ULL, 0x8000000000008003ULL,
    0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800aULL, 0x800000008000000aULL,
    0x8000000080008081ULL, 0x8000000000008080ULL,
    0x0000000080000001ULL, 0x8000000080008008ULL
};

/*
 * =============================================================================
 * 3. EL CORAZÓN DEL CAOS: LA PERMUTACIÓN Keccak-f[1600]
 * =============================================================================
 * Esta es la máquina trituradora. Toma el estado de 25 palabras (200 bytes) y 
 * ejecuta 24 rondas matemáticas de mezcla a máxima eficiencia en registros físicos.
 */
static void KeccakF1600_StatePermute(uint64_t state[25]) {
    int round;
    uint64_t Aba, Abe, Abi, Abo, Abu;
    uint64_t Aga, Age, Agi, Ago, Agu;
    uint64_t Aka, Ake, Aki, Ako, Aku;
    uint64_t Ama, Ame, Ami, Amo, Amu;
    uint64_t Asa, Ase, Asi, Aso, Asu;
    uint64_t BCa, BCe, BCi, BCo, BCu;
    uint64_t Da, De, Di, Do, Du;
    uint64_t Eba, Ebe, Ebi, Ebo, Ebu;
    uint64_t Ega, Ege, Egi, Ego, Egu;
    uint64_t Eka, Eke, Eki, Eko, Eku;
    uint64_t Ema, Eme, Emi, Emo, Emu;
    uint64_t Esa, Ese, Esi, Eso, Esu;

    // Carga de la RAM estática a los registros del Procesador
    Aba = state[ 0]; Abe = state[ 1]; Abi = state[ 2]; Abo = state[ 3]; Abu = state[ 4];
    Aga = state[ 5]; Age = state[ 6]; Agi = state[ 7]; Ago = state[ 8]; Agu = state[ 9];
    Aka = state[10]; Ake = state[11]; Aki = state[12]; Ako = state[13]; Aku = state[14];
    Ama = state[15]; Ame = state[16]; Ami = state[17]; Amo = state[18]; Amu = state[19];
    Asa = state[20]; Ase = state[21]; Asi = state[22]; Aso = state[23]; Asu = state[24];

    // Bucle desenrollado doble (2 saltos por vuelta = 24 rondas totales)
    for (round = 0; round < 24; round += 2) {
        // --- RONDA PAR ---
        BCa = Aba^Aga^Aka^Ama^Asa; BCe = Abe^Age^Ake^Ame^Ase;
        BCi = Abi^Agi^Aki^Ami^Asi; BCo = Abo^Ago^Ako^Amo^Aso;
        BCu = Abu^Agu^Aku^Amu^Asu;
        Da = BCu^ROTL64(BCe, 1); De = BCa^ROTL64(BCi, 1);
        Di = BCe^ROTL64(BCo, 1); Do = BCi^ROTL64(BCu, 1);
        Du = BCo^ROTL64(BCa, 1);

        Aba ^= Da; BCa = Aba; Age ^= De; BCe = ROTL64(Age, 44);
        Aki ^= Di; BCi = ROTL64(Aki, 43); Amo ^= Do; BCo = ROTL64(Amo, 21);
        Asu ^= Du; BCu = ROTL64(Asu, 14);
        Eba = BCa ^((~BCe)& BCi ); Eba ^= KeccakF_RoundConstants[round];
        Ebe = BCe ^((~BCi)& BCo ); Ebi = BCi ^((~BCo)& BCu );
        Ebo = BCo ^((~BCu)& BCa ); Ebu = BCu ^((~BCa)& BCe );

        Abo ^= Do; BCa = ROTL64(Abo, 28); Agu ^= Du; BCe = ROTL64(Agu, 20);
        Aka ^= Da; BCi = ROTL64(Aka,  3); Ame ^= De; BCo = ROTL64(Ame, 45);
        Asi ^= Di; BCu = ROTL64(Asi, 61);
        Ega = BCa ^((~BCe)& BCi ); Ege = BCe ^((~BCi)& BCo );
        Egi = BCi ^((~BCo)& BCu ); Ego = BCo ^((~BCu)& BCa );
        Egu = BCu ^((~BCa)& BCe );

        Abe ^= De; BCa = ROTL64(Abe,  1); Agi ^= Di; BCe = ROTL64(Agi,  6);
        Ako ^= Do; BCi = ROTL64(Ako, 25); Amu ^= Du; BCo = ROTL64(Amu,  8);
        Asa ^= Da; BCu = ROTL64(Asa, 18);
        Eka = BCa ^((~BCe)& BCi ); Eke = BCe ^((~BCi)& BCo );
        Eki = BCi ^((~BCo)& BCu ); Eko = BCo ^((~BCu)& BCa );
        Eku = BCu ^((~BCa)& BCe );

        Abu ^= Du; BCa = ROTL64(Abu, 27); Aga ^= Da; BCe = ROTL64(Aga, 36);
        Ake ^= De; BCi = ROTL64(Ake, 10); Ami ^= Di; BCo = ROTL64(Ami, 15);
        Aso ^= Do; BCu = ROTL64(Aso, 56);
        Ema = BCa ^((~BCe)& BCi ); Eme = BCe ^((~BCi)& BCo );
        Emi = BCi ^((~BCo)& BCu ); Emo = BCo ^((~BCu)& BCa );
        Emu = BCu ^((~BCa)& BCe );

        Abi ^= Di; BCa = ROTL64(Abi, 62); Ago ^= Do; BCe = ROTL64(Ago, 55);
        Aku ^= Du; BCi = ROTL64(Aku, 39); Ama ^= Da; BCo = ROTL64(Ama, 41);
        Ase ^= De; BCu = ROTL64(Ase,  2);
        Esa = BCa ^((~BCe)& BCi ); Ese = BCe ^((~BCi)& BCo );
        Esi = BCi ^((~BCo)& BCu ); Eso = BCo ^((~BCu)& BCa );
        Esu = BCu ^((~BCa)& BCe );

        // --- RONDA IMPAR ---
        BCa = Eba^Ega^Eka^Ema^Esa; BCe = Ebe^Ege^Eke^Eme^Ese;
        BCi = Ebi^Egi^Eki^Emi^Esi; BCo = Ebo^Ego^Eko^Emo^Eso;
        BCu = Ebu^Egu^Eku^Emu^Esu;
        Da = BCu^ROTL64(BCe, 1); De = BCa^ROTL64(BCi, 1);
        Di = BCe^ROTL64(BCo, 1); Do = BCi^ROTL64(BCu, 1);
        Du = BCo^ROTL64(BCa, 1);

        Eba ^= Da; BCa = Eba; Ege ^= De; BCe = ROTL64(Ege, 44);
        Eki ^= Di; BCi = ROTL64(Eki, 43); Emo ^= Do; BCo = ROTL64(Emo, 21);
        Esu ^= Du; BCu = ROTL64(Esu, 14);
        Aba = BCa ^((~BCe)& BCi ); Aba ^= KeccakF_RoundConstants[round+1];
        Abe = BCe ^((~BCi)& BCo ); Abi = BCi ^((~BCo)& BCu );
        Abo = BCo ^((~BCu)& BCa ); Abu = BCu ^((~BCa)& BCe );

        Ebo ^= Do; BCa = ROTL64(Ebo, 28); Egu ^= Du; BCe = ROTL64(Egu, 20);
        Eka ^= Da; BCi = ROTL64(Eka,  3); Eme ^= De; BCo = ROTL64(Eme, 45);
        Esi ^= Di; BCu = ROTL64(Esi, 61);
        Aga = BCa ^((~BCe)& BCi ); Age = BCe ^((~BCi)& BCo );
        Agi = BCi ^((~BCo)& BCu ); Ago = BCo ^((~BCu)& BCa );
        Agu = BCu ^((~BCa)& BCe );

        Ebe ^= De; BCa = ROTL64(Ebe,  1); Egi ^= Di; BCe = ROTL64(Egi,  6);
        Eko ^= Do; BCi = ROTL64(Eko, 25); Emu ^= Du; BCo = ROTL64(Emu,  8);
        Esa ^= Da; BCu = ROTL64(Esa, 18);
        Aka = BCa ^((~BCe)& BCi ); Ake = BCe ^((~BCi)& BCo );
        Aki = BCi ^((~BCo)& BCu ); Ako = BCo ^((~BCu)& BCa );
        Aku = BCu ^((~BCa)& BCe );

        Ebu ^= Du; BCa = ROTL64(Ebu, 27); Ega ^= Da; BCe = ROTL64(Ega, 36);
        Eke ^= De; BCi = ROTL64(Eke, 10); Emi ^= Di; BCo = ROTL64(Emi, 15);
        Eso ^= Do; BCu = ROTL64(Eso, 56);
        Ama = BCa ^((~BCe)& BCi ); Ame = BCe ^((~BCi)& BCo );
        Ami = BCi ^((~BCo)& BCu ); Amo = BCo ^((~BCu)& BCa );
        Amu = BCu ^((~BCa)& BCe );

        Ebi ^= Di; BCa = ROTL64(Ebi, 62); Ego ^= Do; BCe = ROTL64(Ego, 55);
        Eku ^= Du; BCi = ROTL64(Eku, 39); Ema ^= Da; BCo = ROTL64(Ema, 41);
        Ese ^= De; BCu = ROTL64(Ese,  2);
        Asa = BCa ^((~BCe)& BCi ); Ase = BCe ^((~BCi)& BCo );
        Asi = BCi ^((~BCo)& BCu ); Aso = BCo ^((~BCu)& BCa );
        Asu = BCu ^((~BCa)& BCe );
    }

    // Volcado de retorno a la RAM. La mezcla matemática es irreversible.
    state[ 0] = Aba; state[ 1] = Abe; state[ 2] = Abi; state[ 3] = Abo; state[ 4] = Abu;
    state[ 5] = Aga; state[ 6] = Age; state[ 7] = Agi; state[ 8] = Ago; state[ 9] = Agu;
    state[10] = Aka; state[11] = Ake; state[12] = Aki; state[13] = Ako; state[14] = Aku;
    state[15] = Ama; state[16] = Ame; state[17] = Ami; state[18] = Amo; state[19] = Amu;
    state[20] = Asa; state[21] = Ase; state[22] = Asi; state[23] = Aso; state[24] = Asu;
}

/*
 * =============================================================================
 * 4. FUNCIONES DE CONTROL DE ESPONJA (Absorb & Squeeze)
 * =============================================================================
 * Lógica pura de NIST FIPS 202.
 */

/* 
 * keccak_absorb: Traga los datos bloque a bloque, los inyecta en el estado 
 * mediante XOR, y cuando no queda más fichero, inyecta el Padding de seguridad.
 */
static void keccak_absorb(uint64_t s[25], unsigned int r, const uint8_t *m, size_t mlen, uint8_t p) {
    size_t i;
    uint8_t t[200] = {0}; // Buffer estático para el último bloque y el padding

    // Mientras tengamos datos más grandes que la boca de la esponja (Ratio)
    while (mlen >= r) {
        for (i = 0; i < r / 8; ++i) {
            s[i] ^= load64(m + 8 * i); // Mezclamos 8 bytes de forma segura
        }
        KeccakF1600_StatePermute(s);   // Trituramos
        mlen -= r;
        m += r;
    }

    // El último chorrito de bytes (o si el mensaje entero era diminuto)
    for (i = 0; i < mlen; ++i) {
        t[i] = m[i];
    }
    
    // PADDING OBLIGATORIO:
    // Evita colisiones si un mensaje termina justo donde empieza otro.
    t[i] = p; // p = 0x1F para algoritmos SHAKE
    // Marcador final 0x80 según el estándar
    t[r - 1] |= 128; 

    // Mezclamos el último bloque paddenado
    for (i = 0; i < r / 8; ++i) {
        s[i] ^= load64(t + 8 * i);
    }
    KeccakF1600_StatePermute(s);
}

/* 
 * keccak_squeezeblocks: Cosecha la entropía exprimida.
 * Genera bytes pseudo-aleatorios sin límite agitando la esponja tras cada captura.
 */
static void keccak_squeezeblocks(uint8_t *out, size_t nblocks, unsigned int r, uint64_t s[25]) {
    unsigned int i;
    
    // Mientras necesitemos sacar bloques de la esponja
    while (nblocks > 0) {
        for (i = 0; i < r / 8; ++i) {
            store64(out + 8 * i, s[i]); // Vuelca el titanio a bytes mortales
        }
        out += r;
        --nblocks;
        
        // Si seguimos necesitando más bloques, volvemos a exprimir (permutar)
        if (nblocks > 0) {
            KeccakF1600_StatePermute(s);
        }
    }
}

/*
 * =============================================================================
 * 5. LA API PÚBLICA (Resolución de fips202.h)
 * =============================================================================
 * Enrutadores "simples" para que el usuario no tenga que saberse los 
 * parámetros de Padding (0x1F) ni los radios.
 */

// -----------------------------------------------------------------------------
// SHAKE-128
// -----------------------------------------------------------------------------
void shake128_absorb(keccak_state *state, const uint8_t *in, size_t inlen) {
    // 1. Limpiamos a ceros por completo el tablero (Seguridad vital para no arrastrar basura de la RAM vieja)
    for (int i = 0; i < 25; ++i) {
        state->s[i] = 0;
    }
    // 2. Absorbemos pasándole el ratio 168 fijo y el padding oficial 0x1F para SHAKE
    keccak_absorb(state->s, SHAKE128_RATE, in, inlen, 0x1F);
}

void shake128_squeezeblocks(uint8_t *out, size_t nblocks, keccak_state *state) {
    keccak_squeezeblocks(out, nblocks, SHAKE128_RATE, state->s);
}

// Función Todo-en-Uno (One-Shot): Para cuando no te importa guardar la partida
void shake128(uint8_t *out, size_t outlen, const uint8_t *in, size_t inlen) {
    keccak_state state;
    shake128_absorb(&state, in, inlen); // Traga de golpe
    
    // Calcula cuántos bloques exactos necesitamos exprimir
    size_t nblocks = outlen / SHAKE128_RATE;
    shake128_squeezeblocks(out, nblocks, &state);
    out += nblocks * SHAKE128_RATE;
    outlen -= nblocks * SHAKE128_RATE;
    
    // Si la cantidad de bytes que pediste exprimir no era un múltiplo perfecto de 168 (Ratio)...
    // Exprimimos un último bloque sobre un recipiente temporal y copiamos solo el puñadito que faltaba.
    if (outlen) {
        uint8_t t[SHAKE128_RATE];
        shake128_squeezeblocks(t, 1, &state);
        for (size_t i = 0; i < outlen; ++i) out[i] = t[i];
    }
}

// -----------------------------------------------------------------------------
// SHAKE-256
// -----------------------------------------------------------------------------

void shake256_absorb(keccak_state *state, const uint8_t *in, size_t inlen) {
    for (int i = 0; i < 25; ++i) {
        state->s[i] = 0;
    }
    keccak_absorb(state->s, SHAKE256_RATE, in, inlen, 0x1F);
}

void shake256_squeezeblocks(uint8_t *out, size_t nblocks, keccak_state *state) {
    keccak_squeezeblocks(out, nblocks, SHAKE256_RATE, state->s);
}

void shake256(uint8_t *out, size_t outlen, const uint8_t *in, size_t inlen) {
    keccak_state state;
    shake256_absorb(&state, in, inlen);
    
    size_t nblocks = outlen / SHAKE256_RATE;
    shake256_squeezeblocks(out, nblocks, &state);
    out += nblocks * SHAKE256_RATE;
    outlen -= nblocks * SHAKE256_RATE;
    
    if (outlen) {
        uint8_t t[SHAKE256_RATE];
        shake256_squeezeblocks(t, 1, &state);
        for (size_t i = 0; i < outlen; ++i) out[i] = t[i];
    }
}
