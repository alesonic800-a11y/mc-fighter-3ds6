# Alesonic — nuevo luchador

Personaje añadido como índice 5 (sexto luchador), sin reemplazar a los cinco originales.

## Diseño
- Pelo castaño, barba, ojos azul oscuro.
- Camiseta turquesa, pantalón azul y zapatos grises.
- Paleta tomada de la imagen de referencia.

## Estadísticas
- Vida: 102
- Velocidad: 4.8
- Fuerza: 1.05

## Movimientos
- A — Puño normal (animación `punch`).
- B — Patada (animación `kick`).
- Abajo + B — Patada baja (`low`).
- X — **Puño turquesa**: proyectil de energía (`sp1` / `proj`).
- Y — **Giro pixelado**: ataque especial de alcance cercano (`sp2`).
- R con la barra llena — **Supremo bloque** (`super`).

## Recursos
`romfs/sprites.bin` contiene 24 poses de combate y el retrato del personaje.
`gfx/alesonic_spritesheet.png` es una hoja de referencia con las 24 poses.
Los sonidos específicos están en `romfs/audio/alesonic_*.wav`.
`romfs/audio/nombre_alesonic.wav` es una fanfarria electrónica de presentación,
no una voz humana grabada.

## Selección
El menú ahora muestra los seis personajes en una fila. El modo arcade enfrenta
al jugador contra los otros cinco luchadores.
