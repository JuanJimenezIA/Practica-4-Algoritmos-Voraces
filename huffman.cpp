// Compresión Huffman real a nivel de bits
// Compilar: g++ -std=c++17 -O2 huffman_bits.cpp -o huffman
// Uso:      ./huffman c entrada salida.huf   (comprimir)
//           ./huffman d entrada.huf salida   (descomprimir)

#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <queue>
#include <stdexcept>
#include <string>
#include <vector>

// ============================================================
//  BitWriter: empaqueta bits en un buffer de uint8_t
// ============================================================
class BitWriter {
public:
    // Escribe un bit (0 o 1) en el byte actual, de izquierda a derecha (MSB primero)
    void escribirBit(uint8_t bit) {
        byteActual = static_cast<uint8_t>((byteActual << 1) | (bit & 1u));
        ++bitsEnByte;
        if (bitsEnByte == 8) {           // byte completo -> al buffer
            buffer.push_back(byteActual);
            byteActual = 0;
            bitsEnByte = 0;
        }
    }

    // Escribe los 'n' bits menos significativos de 'valor', del más alto al más bajo
    void escribirBits(uint64_t valor, int n) {
        for (int i = n - 1; i >= 0; --i)
            escribirBit(static_cast<uint8_t>((valor >> i) & 1u));
    }

    // Completa el último byte con ceros a la derecha (relleno)
    // y devuelve cuántos bits de relleno se agregaron
    int vaciar() {
        int relleno = 0;
        if (bitsEnByte > 0) {
            relleno = 8 - bitsEnByte;
            byteActual = static_cast<uint8_t>(byteActual << relleno);
            buffer.push_back(byteActual);
            byteActual = 0;
            bitsEnByte = 0;
        }
        return relleno;
    }

    const std::vector<uint8_t>& datos() const { return buffer; }

private:
    std::vector<uint8_t> buffer;
    uint8_t byteActual = 0;
    int bitsEnByte = 0;
};

// ============================================================
//  BitReader: recupera los bits de un buffer de uint8_t
// ============================================================
class BitReader {
public:
    BitReader(const std::vector<uint8_t>& datos, std::size_t inicio = 0)
        : buffer(datos), posByte(inicio) {}

    // Lee un bit; devuelve false si ya no hay más datos
    bool leerBit(uint8_t& bit) {
        if (posByte >= buffer.size()) return false;
        bit = static_cast<uint8_t>((buffer[posByte] >> (7 - posBit)) & 1u);
        ++posBit;
        if (posBit == 8) {
            posBit = 0;
            ++posByte;
        }
        return true;
    }

private:
    const std::vector<uint8_t>& buffer;
    std::size_t posByte;
    int posBit = 0;
};

// ============================================================
//  Árbol de Huffman
// ============================================================
struct Nodo {
    uint8_t simbolo = 0;
    uint64_t frecuencia = 0;
    int orden = 0;                       // desempate determinista
    std::unique_ptr<Nodo> izq, der;

    Nodo(uint8_t s, uint64_t f, int o) : simbolo(s), frecuencia(f), orden(o) {}
    Nodo(std::unique_ptr<Nodo> a, std::unique_ptr<Nodo> b, int o)
        : frecuencia(a->frecuencia + b->frecuencia), orden(o),
          izq(std::move(a)), der(std::move(b)) {}

    bool esHoja() const { return !izq && !der; }
};

struct CompararNodo {
    bool operator()(const Nodo* a, const Nodo* b) const {
        if (a->frecuencia != b->frecuencia) return a->frecuencia > b->frecuencia;
        return a->orden > b->orden;
    }
};

// Construye el árbol a partir de la tabla de frecuencias.
// Compresor y descompresor usan esta misma función, así obtienen el mismo árbol.
std::unique_ptr<Nodo> construirArbol(const std::vector<uint64_t>& freq) {
    std::priority_queue<Nodo*, std::vector<Nodo*>, CompararNodo> cola;
    for (int i = 0; i < 256; ++i)
        if (freq[i] > 0)
            cola.push(new Nodo(static_cast<uint8_t>(i), freq[i], i));

    if (cola.empty()) return nullptr;

    int siguienteOrden = 256;
    while (cola.size() > 1) {
        std::unique_ptr<Nodo> a(cola.top()); cola.pop();
        std::unique_ptr<Nodo> b(cola.top()); cola.pop();
        cola.push(new Nodo(std::move(a), std::move(b), siguienteOrden++));
    }
    return std::unique_ptr<Nodo>(cola.top());
}

// Código de Huffman como bits dentro de un entero (NO como texto)
struct Codigo {
    uint64_t bits = 0;
    int longitud = 0;
};

void generarCodigos(const Nodo* nodo, uint64_t bits, int longitud,
                    std::vector<Codigo>& tabla) {
    if (!nodo) return;
    if (nodo->esHoja()) {
        tabla[nodo->simbolo] = {bits, longitud};
        return;
    }
    if (longitud >= 64)
        throw std::runtime_error("Código de Huffman mayor a 64 bits");
    generarCodigos(nodo->izq.get(), (bits << 1) | 0u, longitud + 1, tabla);
    generarCodigos(nodo->der.get(), (bits << 1) | 1u, longitud + 1, tabla);
}

// ============================================================
//  Utilidades de E/S binaria (enteros con desplazamientos y máscaras)
// ============================================================
std::vector<uint8_t> leerArchivo(const std::string& ruta) {
    std::ifstream in(ruta, std::ios::binary);
    if (!in) throw std::runtime_error("No se pudo abrir: " + ruta);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
}

void escribirArchivo(const std::string& ruta, const std::vector<uint8_t>& datos) {
    std::ofstream out(ruta, std::ios::binary);
    if (!out) throw std::runtime_error("No se pudo crear: " + ruta);
    out.write(reinterpret_cast<const char*>(datos.data()),
              static_cast<std::streamsize>(datos.size()));
}

// Escribe un entero de 'nBytes' bytes en little-endian
void escribirEntero(std::vector<uint8_t>& out, uint64_t valor, int nBytes) {
    for (int i = 0; i < nBytes; ++i)
        out.push_back(static_cast<uint8_t>((valor >> (8 * i)) & 0xFFu));
}

uint64_t leerEntero(const std::vector<uint8_t>& in, std::size_t& pos, int nBytes) {
    if (pos + nBytes > in.size())
        throw std::runtime_error("Archivo comprimido truncado");
    uint64_t valor = 0;
    for (int i = 0; i < nBytes; ++i)
        valor |= static_cast<uint64_t>(in[pos++]) << (8 * i);
    return valor;
}

// ============================================================
//  Formato del archivo .huf
//  [4]  firma "HUF1"
//  [8]  tamaño original en bytes
//  [2]  número de símbolos distintos (K)
//  K × ([1] símbolo + [8] frecuencia)
//  [..] bits comprimidos (el último byte lleva relleno de ceros)
// ============================================================
void comprimir(const std::string& rutaEntrada, const std::string& rutaSalida) {
    std::vector<uint8_t> datos = leerArchivo(rutaEntrada);

    // 1. Frecuencias
    std::vector<uint64_t> freq(256, 0);
    for (uint8_t c : datos) ++freq[c];

    // 2. Árbol y tabla de códigos
    std::unique_ptr<Nodo> raiz = construirArbol(freq);
    std::vector<Codigo> tabla(256);
    generarCodigos(raiz.get(), 0, 0, tabla);

    // 3. Encabezado
    std::vector<uint8_t> salida = {'H', 'U', 'F', '1'};
    escribirEntero(salida, datos.size(), 8);
    int distintos = 0;
    for (int i = 0; i < 256; ++i) if (freq[i] > 0) ++distintos;
    escribirEntero(salida, static_cast<uint64_t>(distintos), 2);
    for (int i = 0; i < 256; ++i) {
        if (freq[i] > 0) {
            salida.push_back(static_cast<uint8_t>(i));
            escribirEntero(salida, freq[i], 8);
        }
    }
    std::size_t tamEncabezado = salida.size();

    // 4. Datos comprimidos bit a bit
    //    (si solo hay un símbolo distinto, su código mide 0 bits:
    //     basta con el tamaño original para reconstruirlo)
    BitWriter bw;
    for (uint8_t c : datos)
        bw.escribirBits(tabla[c].bits, tabla[c].longitud);
    int relleno = bw.vaciar();

    const std::vector<uint8_t>& bits = bw.datos();
    salida.insert(salida.end(), bits.begin(), bits.end());
    escribirArchivo(rutaSalida, salida);

    std::cout << "Tamaño original:    " << datos.size() << " bytes\n"
              << "Encabezado:         " << tamEncabezado << " bytes\n"
              << "Datos comprimidos:  " << bits.size() << " bytes ("
              << relleno << " bits de relleno)\n"
              << "Tamaño final:       " << salida.size() << " bytes\n";
    if (!datos.empty())
        std::cout << "Razón de compresión: "
                  << 100.0 * salida.size() / datos.size() << " %\n";
}

void descomprimir(const std::string& rutaEntrada, const std::string& rutaSalida) {
    std::vector<uint8_t> in = leerArchivo(rutaEntrada);
    std::size_t pos = 0;

    // 1. Encabezado
    if (in.size() < 4 || in[0] != 'H' || in[1] != 'U' || in[2] != 'F' || in[3] != '1')
        throw std::runtime_error("No es un archivo HUF1 válido");
    pos = 4;
    uint64_t tamOriginal = leerEntero(in, pos, 8);
    uint64_t distintos   = leerEntero(in, pos, 2);

    std::vector<uint64_t> freq(256, 0);
    for (uint64_t k = 0; k < distintos; ++k) {
        uint8_t s = static_cast<uint8_t>(leerEntero(in, pos, 1));
        freq[s] = leerEntero(in, pos, 8);
    }

    // 2. Reconstruir el mismo árbol
    std::unique_ptr<Nodo> raiz = construirArbol(freq);

    // 3. Decodificar recorriendo el árbol bit a bit
    std::vector<uint8_t> salida;
    salida.reserve(tamOriginal);

    if (raiz && raiz->esHoja()) {
        // Un solo símbolo distinto
        salida.assign(tamOriginal, raiz->simbolo);
    } else if (raiz) {
        BitReader br(in, pos);
        // Se detiene al recuperar tamOriginal símbolos:
        // así los bits de relleno del último byte nunca se decodifican.
        while (salida.size() < tamOriginal) {
            const Nodo* n = raiz.get();
            while (!n->esHoja()) {
                uint8_t bit;
                if (!br.leerBit(bit))
                    throw std::runtime_error("Datos comprimidos incompletos");
                n = (bit == 0) ? n->izq.get() : n->der.get();
            }
            salida.push_back(n->simbolo);
        }
    }

    escribirArchivo(rutaSalida, salida);
    std::cout << "Bytes recuperados: " << salida.size() << '\n';
}

// ============================================================
int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Uso:\n"
                  << "  " << argv[0] << " c <entrada> <salida.huf>\n"
                  << "  " << argv[0] << " d <entrada.huf> <salida>\n";
        return 1;
    }
    try {
        std::string modo = argv[1];
        if (modo == "c")      comprimir(argv[2], argv[3]);
        else if (modo == "d") descomprimir(argv[2], argv[3]);
        else throw std::runtime_error("Modo no válido (usa c o d)");
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}