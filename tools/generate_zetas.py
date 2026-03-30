Q = 8380417
ZETA = 1753
N = 256

def bit_reverse(n, bits=8):
    # Convierte a binario, rellena con ceros, invierte la cadena y vuelve a entero
    return int(bin(n)[2:].zfill(bits)[::-1], 2)

print("const int32_t zetas[256] = {")

for i in range(N):
    # 1. Calcular el índice permutado (Bit-Reversal)
    br_i = bit_reverse(i)
    
    # 2. Elevar la raíz de la unidad a la potencia permutada
    z_power = pow(ZETA, br_i, Q)
    
    # 3. Trasladar la constante al dominio de Montgomery ( x * 2^32 mod Q )
    # Esto es vital para que poly_ntt() use montgomery_reduce sin conversiones extra
    z_mont = (z_power * (1 << 32)) % Q
    
    # Formatear la salida para C
    terminador = "," if i < N - 1 else ""
    print(f"    {z_mont}{terminador} // Indice real: {i}, Potencia original: {br_i}")

print("};")