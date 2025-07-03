#ifndef CONFIG_H
#define CONFIG_H

// Configuración de sensibilidad del cursor
// Umbral de velocidad mínima (distancia^2 en 50ms) para que un movimiento cuente.
// Usa el cuadrado para evitar std::sqrt en cada punto.
// Un valor de 15^2 = 225 significaría un movimiento de al menos 15 píxeles en 50ms.
// Ajusta esto según la sensibilidad deseada y la resolución de pantalla.
#define UMBRAL 20.0
#define UMBRAL_VELOCIDAD_MIN_CUADRADO UMBRAL*UMBRAL // Ejemplo: 20 px/50ms



// Umbral del coseno del ángulo entre dos vectores de movimiento consecutivos
// para considerar que hubo una "reversión" significativa de dirección.
// -1.0 es 180 grados (dirección exactamente opuesta).
// -0.7 es un ángulo mayor a 134 grados (más de 90 grados opuesto).
// -0.5 es un ángulo mayor a 120 grados.
// Ejemplo: Considerar reversiones fuertes
#define UMBRAL_COSENO_REVERSION -0.7



// Umbral total del "puntaje de sacudida" acumulado en la ventana de historial
// para disparar el evento. Este valor depende de cómo calculas el puntaje individual
// (aquí, la suma de magnitudes de los segmentos que cumplen los umbrales).
// Un valor de 200 podría significar movimientos rápidos que suman al menos 200 píxeles
// en total durante las reversiones dentro de la ventana.
#define UMBRAL_SACUDIDA_TOTAL  250.0 // Ejemplo: Suma total de distancias en reversiones


// Tiempo en milisegundos durante el cual el detector estará inactivo después de una sacudida.
// Esto evita múltiples activaciones por el mismo movimiento o temblores posteriores.
#define TIEMPO_ENFRIAMIENTO_MS 6500*2 // 1.5 segundos


#define DISTANCE_SENSIBILITY 250 // Entre más alto, menos sensible
#define TIME_TO_REVERT 2000        // Tiempo para revertir tamaño del cursor (ms)

// Configuración del dock
#define DOCK_HEIGHT 100 // Altura del dock en píxeles
#define NUM_ELEMENTOS 30 // Altura del dock en píxeles

//El porcentaje inferior en el cual se mostrara el dock
#define AREA_DE_MUESTRA 95

#define FRECUENCIA_MS 100

#endif // CONFIG_H
