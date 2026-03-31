#include "ntt.h"
#include "arithmetic.h"

const int32_t zetas[256] = {
    4193792, // Indice real: 0, Potencia original: 0
    25847, // Indice real: 1, Potencia original: 128
    5771523, // Indice real: 2, Potencia original: 64
    7861508, // Indice real: 3, Potencia original: 192
    237124, // Indice real: 4, Potencia original: 32
    7602457, // Indice real: 5, Potencia original: 160
    7504169, // Indice real: 6, Potencia original: 96
    466468, // Indice real: 7, Potencia original: 224
    1826347, // Indice real: 8, Potencia original: 16
    2353451, // Indice real: 9, Potencia original: 144
    8021166, // Indice real: 10, Potencia original: 80
    6288512, // Indice real: 11, Potencia original: 208
    3119733, // Indice real: 12, Potencia original: 48
    5495562, // Indice real: 13, Potencia original: 176
    3111497, // Indice real: 14, Potencia original: 112
    2680103, // Indice real: 15, Potencia original: 240
    2725464, // Indice real: 16, Potencia original: 8
    1024112, // Indice real: 17, Potencia original: 136
    7300517, // Indice real: 18, Potencia original: 72
    3585928, // Indice real: 19, Potencia original: 200
    7830929, // Indice real: 20, Potencia original: 40
    7260833, // Indice real: 21, Potencia original: 168
    2619752, // Indice real: 22, Potencia original: 104
    6271868, // Indice real: 23, Potencia original: 232
    6262231, // Indice real: 24, Potencia original: 24
    4520680, // Indice real: 25, Potencia original: 152
    6980856, // Indice real: 26, Potencia original: 88
    5102745, // Indice real: 27, Potencia original: 216
    1757237, // Indice real: 28, Potencia original: 56
    8360995, // Indice real: 29, Potencia original: 184
    4010497, // Indice real: 30, Potencia original: 120
    280005, // Indice real: 31, Potencia original: 248
    2706023, // Indice real: 32, Potencia original: 4
    95776, // Indice real: 33, Potencia original: 132
    3077325, // Indice real: 34, Potencia original: 68
    3530437, // Indice real: 35, Potencia original: 196
    6718724, // Indice real: 36, Potencia original: 36
    4788269, // Indice real: 37, Potencia original: 164
    5842901, // Indice real: 38, Potencia original: 100
    3915439, // Indice real: 39, Potencia original: 228
    4519302, // Indice real: 40, Potencia original: 20
    5336701, // Indice real: 41, Potencia original: 148
    3574422, // Indice real: 42, Potencia original: 84
    5512770, // Indice real: 43, Potencia original: 212
    3539968, // Indice real: 44, Potencia original: 52
    8079950, // Indice real: 45, Potencia original: 180
    2348700, // Indice real: 46, Potencia original: 116
    7841118, // Indice real: 47, Potencia original: 244
    6681150, // Indice real: 48, Potencia original: 12
    6736599, // Indice real: 49, Potencia original: 140
    3505694, // Indice real: 50, Potencia original: 76
    4558682, // Indice real: 51, Potencia original: 204
    3507263, // Indice real: 52, Potencia original: 44
    6239768, // Indice real: 53, Potencia original: 172
    6779997, // Indice real: 54, Potencia original: 108
    3699596, // Indice real: 55, Potencia original: 236
    811944, // Indice real: 56, Potencia original: 28
    531354, // Indice real: 57, Potencia original: 156
    954230, // Indice real: 58, Potencia original: 92
    3881043, // Indice real: 59, Potencia original: 220
    3900724, // Indice real: 60, Potencia original: 60
    5823537, // Indice real: 61, Potencia original: 188
    2071892, // Indice real: 62, Potencia original: 124
    5582638, // Indice real: 63, Potencia original: 252
    4450022, // Indice real: 64, Potencia original: 2
    6851714, // Indice real: 65, Potencia original: 130
    4702672, // Indice real: 66, Potencia original: 66
    5339162, // Indice real: 67, Potencia original: 194
    6927966, // Indice real: 68, Potencia original: 34
    3475950, // Indice real: 69, Potencia original: 162
    2176455, // Indice real: 70, Potencia original: 98
    6795196, // Indice real: 71, Potencia original: 226
    7122806, // Indice real: 72, Potencia original: 18
    1939314, // Indice real: 73, Potencia original: 146
    4296819, // Indice real: 74, Potencia original: 82
    7380215, // Indice real: 75, Potencia original: 210
    5190273, // Indice real: 76, Potencia original: 50
    5223087, // Indice real: 77, Potencia original: 178
    4747489, // Indice real: 78, Potencia original: 114
    126922, // Indice real: 79, Potencia original: 242
    3412210, // Indice real: 80, Potencia original: 10
    7396998, // Indice real: 81, Potencia original: 138
    2147896, // Indice real: 82, Potencia original: 74
    2715295, // Indice real: 83, Potencia original: 202
    5412772, // Indice real: 84, Potencia original: 42
    4686924, // Indice real: 85, Potencia original: 170
    7969390, // Indice real: 86, Potencia original: 106
    5903370, // Indice real: 87, Potencia original: 234
    7709315, // Indice real: 88, Potencia original: 26
    7151892, // Indice real: 89, Potencia original: 154
    8357436, // Indice real: 90, Potencia original: 90
    7072248, // Indice real: 91, Potencia original: 218
    7998430, // Indice real: 92, Potencia original: 58
    1349076, // Indice real: 93, Potencia original: 186
    1852771, // Indice real: 94, Potencia original: 122
    6949987, // Indice real: 95, Potencia original: 250
    5037034, // Indice real: 96, Potencia original: 6
    264944, // Indice real: 97, Potencia original: 134
    508951, // Indice real: 98, Potencia original: 70
    3097992, // Indice real: 99, Potencia original: 198
    44288, // Indice real: 100, Potencia original: 38
    7280319, // Indice real: 101, Potencia original: 166
    904516, // Indice real: 102, Potencia original: 102
    3958618, // Indice real: 103, Potencia original: 230
    4656075, // Indice real: 104, Potencia original: 22
    8371839, // Indice real: 105, Potencia original: 150
    1653064, // Indice real: 106, Potencia original: 86
    5130689, // Indice real: 107, Potencia original: 214
    2389356, // Indice real: 108, Potencia original: 54
    8169440, // Indice real: 109, Potencia original: 182
    759969, // Indice real: 110, Potencia original: 118
    7063561, // Indice real: 111, Potencia original: 246
    189548, // Indice real: 112, Potencia original: 14
    4827145, // Indice real: 113, Potencia original: 142
    3159746, // Indice real: 114, Potencia original: 78
    6529015, // Indice real: 115, Potencia original: 206
    5971092, // Indice real: 116, Potencia original: 46
    8202977, // Indice real: 117, Potencia original: 174
    1315589, // Indice real: 118, Potencia original: 110
    1341330, // Indice real: 119, Potencia original: 238
    1285669, // Indice real: 120, Potencia original: 30
    6795489, // Indice real: 121, Potencia original: 158
    7567685, // Indice real: 122, Potencia original: 94
    6940675, // Indice real: 123, Potencia original: 222
    5361315, // Indice real: 124, Potencia original: 62
    4499357, // Indice real: 125, Potencia original: 190
    4751448, // Indice real: 126, Potencia original: 126
    3839961, // Indice real: 127, Potencia original: 254
    2091667, // Indice real: 128, Potencia original: 1
    3407706, // Indice real: 129, Potencia original: 129
    2316500, // Indice real: 130, Potencia original: 65
    3817976, // Indice real: 131, Potencia original: 193
    5037939, // Indice real: 132, Potencia original: 33
    2244091, // Indice real: 133, Potencia original: 161
    5933984, // Indice real: 134, Potencia original: 97
    4817955, // Indice real: 135, Potencia original: 225
    266997, // Indice real: 136, Potencia original: 17
    2434439, // Indice real: 137, Potencia original: 145
    7144689, // Indice real: 138, Potencia original: 81
    3513181, // Indice real: 139, Potencia original: 209
    4860065, // Indice real: 140, Potencia original: 49
    4621053, // Indice real: 141, Potencia original: 177
    7183191, // Indice real: 142, Potencia original: 113
    5187039, // Indice real: 143, Potencia original: 241
    900702, // Indice real: 144, Potencia original: 9
    1859098, // Indice real: 145, Potencia original: 137
    909542, // Indice real: 146, Potencia original: 73
    819034, // Indice real: 147, Potencia original: 201
    495491, // Indice real: 148, Potencia original: 41
    6767243, // Indice real: 149, Potencia original: 169
    8337157, // Indice real: 150, Potencia original: 105
    7857917, // Indice real: 151, Potencia original: 233
    7725090, // Indice real: 152, Potencia original: 25
    5257975, // Indice real: 153, Potencia original: 153
    2031748, // Indice real: 154, Potencia original: 89
    3207046, // Indice real: 155, Potencia original: 217
    4823422, // Indice real: 156, Potencia original: 57
    7855319, // Indice real: 157, Potencia original: 185
    7611795, // Indice real: 158, Potencia original: 121
    4784579, // Indice real: 159, Potencia original: 249
    342297, // Indice real: 160, Potencia original: 5
    286988, // Indice real: 161, Potencia original: 133
    5942594, // Indice real: 162, Potencia original: 69
    4108315, // Indice real: 163, Potencia original: 197
    3437287, // Indice real: 164, Potencia original: 37
    5038140, // Indice real: 165, Potencia original: 165
    1735879, // Indice real: 166, Potencia original: 101
    203044, // Indice real: 167, Potencia original: 229
    2842341, // Indice real: 168, Potencia original: 21
    2691481, // Indice real: 169, Potencia original: 149
    5790267, // Indice real: 170, Potencia original: 85
    1265009, // Indice real: 171, Potencia original: 213
    4055324, // Indice real: 172, Potencia original: 53
    1247620, // Indice real: 173, Potencia original: 181
    2486353, // Indice real: 174, Potencia original: 117
    1595974, // Indice real: 175, Potencia original: 245
    4613401, // Indice real: 176, Potencia original: 13
    1250494, // Indice real: 177, Potencia original: 141
    2635921, // Indice real: 178, Potencia original: 77
    4832145, // Indice real: 179, Potencia original: 205
    5386378, // Indice real: 180, Potencia original: 45
    1869119, // Indice real: 181, Potencia original: 173
    1903435, // Indice real: 182, Potencia original: 109
    7329447, // Indice real: 183, Potencia original: 237
    7047359, // Indice real: 184, Potencia original: 29
    1237275, // Indice real: 185, Potencia original: 157
    5062207, // Indice real: 186, Potencia original: 93
    6950192, // Indice real: 187, Potencia original: 221
    7929317, // Indice real: 188, Potencia original: 61
    1312455, // Indice real: 189, Potencia original: 189
    3306115, // Indice real: 190, Potencia original: 125
    6417775, // Indice real: 191, Potencia original: 253
    7100756, // Indice real: 192, Potencia original: 3
    1917081, // Indice real: 193, Potencia original: 131
    5834105, // Indice real: 194, Potencia original: 67
    7005614, // Indice real: 195, Potencia original: 195
    1500165, // Indice real: 196, Potencia original: 35
    777191, // Indice real: 197, Potencia original: 163
    2235880, // Indice real: 198, Potencia original: 99
    3406031, // Indice real: 199, Potencia original: 227
    7838005, // Indice real: 200, Potencia original: 19
    5548557, // Indice real: 201, Potencia original: 147
    6709241, // Indice real: 202, Potencia original: 83
    6533464, // Indice real: 203, Potencia original: 211
    5796124, // Indice real: 204, Potencia original: 51
    4656147, // Indice real: 205, Potencia original: 179
    594136, // Indice real: 206, Potencia original: 115
    4603424, // Indice real: 207, Potencia original: 243
    6366809, // Indice real: 208, Potencia original: 11
    2432395, // Indice real: 209, Potencia original: 139
    2454455, // Indice real: 210, Potencia original: 75
    8215696, // Indice real: 211, Potencia original: 203
    1957272, // Indice real: 212, Potencia original: 43
    3369112, // Indice real: 213, Potencia original: 171
    185531, // Indice real: 214, Potencia original: 107
    7173032, // Indice real: 215, Potencia original: 235
    5196991, // Indice real: 216, Potencia original: 27
    162844, // Indice real: 217, Potencia original: 155
    1616392, // Indice real: 218, Potencia original: 91
    3014001, // Indice real: 219, Potencia original: 219
    810149, // Indice real: 220, Potencia original: 59
    1652634, // Indice real: 221, Potencia original: 187
    4686184, // Indice real: 222, Potencia original: 123
    6581310, // Indice real: 223, Potencia original: 251
    5341501, // Indice real: 224, Potencia original: 7
    3523897, // Indice real: 225, Potencia original: 135
    3866901, // Indice real: 226, Potencia original: 71
    269760, // Indice real: 227, Potencia original: 199
    2213111, // Indice real: 228, Potencia original: 39
    7404533, // Indice real: 229, Potencia original: 167
    1717735, // Indice real: 230, Potencia original: 103
    472078, // Indice real: 231, Potencia original: 231
    7953734, // Indice real: 232, Potencia original: 23
    1723600, // Indice real: 233, Potencia original: 151
    6577327, // Indice real: 234, Potencia original: 87
    1910376, // Indice real: 235, Potencia original: 215
    6712985, // Indice real: 236, Potencia original: 55
    7276084, // Indice real: 237, Potencia original: 183
    8119771, // Indice real: 238, Potencia original: 119
    4546524, // Indice real: 239, Potencia original: 247
    5441381, // Indice real: 240, Potencia original: 15
    6144432, // Indice real: 241, Potencia original: 143
    7959518, // Indice real: 242, Potencia original: 79
    6094090, // Indice real: 243, Potencia original: 207
    183443, // Indice real: 244, Potencia original: 47
    7403526, // Indice real: 245, Potencia original: 175
    1612842, // Indice real: 246, Potencia original: 111
    4834730, // Indice real: 247, Potencia original: 239
    7826001, // Indice real: 248, Potencia original: 31
    3919660, // Indice real: 249, Potencia original: 159
    8332111, // Indice real: 250, Potencia original: 95
    7018208, // Indice real: 251, Potencia original: 223
    3937738, // Indice real: 252, Potencia original: 63
    1400424, // Indice real: 253, Potencia original: 191
    7534263, // Indice real: 254, Potencia original: 127
    1976782 // Indice real: 255, Potencia original: 255
};

extern int32_t montgomery_reduce(int64_t a); 

void poly_ntt(int32_t a[256]) {
    unsigned int len, start, j, k;
    int32_t zeta, t;

    k = 1; // Índice secuencial para leer las zetas de la memoria Flash

    // Bucle 1: Las 8 capas (Controla la distancia de la mariposa)
    for (len = 128; len >= 1; len >>= 1) { 
        
        // Bucle 2: Recorre los diferentes bloques independientes en la capa actual
        for (start = 0; start < 256; start = j + len) { 
            
            // Leemos el factor W (la hora del reloj pre-desordenada)
            zeta = zetas[k++]; 
            
            // Bucle 3: La operación Mariposa (El cálculo matemático puro)
            for (j = start; j < start + len; ++j) { 
                
                // 1. Producto temporal (t = Abajo * W)
                // Hacemos cast a 64 bits para que la multiplicación no desborde antes de reducir
                t = montgomery_reduce((int64_t)zeta * a[j + len]); 
                
                // 2. Nuevo Abajo = Arriba - t
                a[j + len] = a[j] - t; 
                
                // 3. Nuevo Arriba = Arriba + t
                a[j] = a[j] + t; 
            }
        }
    }
}