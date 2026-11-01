# Integración de audio original

Esta versión incorpora reproducción de audio mediante NDSP/libctru para Nintendo 3DS.

## Audio integrado
- Música de menú: `romfs/audio/musica_menu.wav` (bucle).
- Música de combate de Cibeles: `romfs/audio/musica_cibeles.wav` (bucle).
- Efectos: puñetazos, patadas, golpes fuertes, bloqueos, movimientos especiales y caídas.
- Voces: presentación de los cinco luchadores del port, título, selección, inicio del combate, KO y victoria.

Los recursos usados se convirtieron de los MP3/M4A recuperados del HAR a WAV PCM estéreo de 16 bits y 22.05 kHz. El gestor carga los WAV desde `romfs:/audio/` bajo demanda. La música usa un canal NDSP en bucle; efectos y voces usan canales separados.

## Archivos modificados
- `source/audio.c` y `source/audio.h`: inicialización NDSP, lectura WAV PCM y reproducción.
- `source/main.c`: llamadas de audio ligadas a menú, selección, combate, golpes, bloqueo, KO y victoria.
- `Makefile`: compila `source/audio.c` y empaqueta los recursos anidados de `romfs/`.
- `romfs/audio/`: 24 WAV usados por el juego.

## Verificación
- Compilación de las pruebas de host: correcta.
- Ejecución de la prueba de host: correcta.
- Prueba de estrés con AddressSanitizer/UndefinedBehaviorSanitizer: correcta (50.000 frames).
- No se pudo compilar aquí el binario 3DS/CIA porque este entorno no tiene `devkitARM`/`devkitPro`. El workflow de GitHub Actions incluido ejecuta `make cia` y `make 3dsx` en el contenedor oficial de devkitPro.

## Compilar
En GitHub, sube el contenido de este proyecto y abre **Actions → Compilar 3DS → Run workflow**. Descarga el artefacto `moncloas-fighter-3ds`, que contendrá el `.cia` y el `.3dsx` si la compilación termina correctamente.
