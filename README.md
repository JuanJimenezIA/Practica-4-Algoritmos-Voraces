# Práctica 4. Algoritmos Voraces y Codificación Huffman

**Unidad de aprendizaje:** Análisis y Diseño de Algoritmos
**Profesor:** Andrés García Floriano
**Grupo:** 3BV1

**Integrantes:**
- Jiménez Jiménez Juan Manuel
- Sanches Leyva Gerardo

---

## Descripción

Compresor y descompresor de archivos con el algoritmo de Huffman, escrito en C++17.

Los bits se empaquetan de verdad en bytes con las clases `BitWriter` y `BitReader`, usando desplazamientos (`<<`, `>>`) y máscaras (`&`, `|`), en lugar de guardarlos como texto `"0101"`.

El archivo comprimido guarda en su cabecera el número total de caracteres originales (entero de 64 bits). Así el descompresor sabe cuándo detenerse y no confunde los ceros de relleno del último byte con caracteres reales.

## Contenido del repositorio

| Archivo | Descripción |
|---|---|
| `huffman.cpp` | Código fuente del compresor/descompresor |
| `Practica_4_JJJM_SLG.pdf` | Reporte de la práctica (Fase 1 y Fase 2) |

## Conversación con la IA

Conversación completa con la herramienta de IA usada en la Fase 2:
[https://claude.ai/share/dcadd9a9-4b4e-4a16-9a62-18753b0a71c8](https://claude.ai/share/dcadd9a9-4b4e-4a16-9a62-18753b0a71c8)

## Compilación

```bash
g++ -std=c++17 -O2 huffman.cpp -o huffman
```

## Uso

```bash
# Comprimir
./huffman c archivo_entrada archivo_salida.huf

# Descomprimir
./huffman d archivo_entrada.huf archivo_salida
```

## Resultados del benchmarking

| Prueba | Tamaño original | Tamaño comprimido | Razón de compresión |
|---|---|---|---|
| Código fuente (`huffman.cpp`) | 11,169 bytes | 7,657 bytes | 68.56 % |
| Archivo ya comprimido (`.zip`) | 77,427 bytes | 79,734 bytes | 102.98 % |
| 10,000,000 de letras `'A'` | 10,000,000 bytes | 23 bytes | 0.00023 % |

- El texto se comprime porque tiene caracteres que se repiten mucho.
- El `.zip` crece porque ya está comprimido (entropía cercana a 8 bits por byte) y además se suma la cabecera.
- El archivo de un solo símbolo no falla: solo se guarda el carácter y cuántas veces se repite.
