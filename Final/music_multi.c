/*
Implementación de MUSIC multi fuente como la primera parte 
 del proyecto final
Eduardo García Alarcón 05/2024
Procesamiento Digital de Audio
PCIC - IIMAS
*/

/// Bibliotecas
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
// JACK
#include <jack/jack.h>
// FFTW3
#include <complex.h>
#include <fftw3.h>

///////////// Varibales globales ///////
const double vel_sonido = 343;   // metros/segundos
const double dist_mic   = 0.18;  // Distancia entre micrófonos, checar AIRA
const int mic_n         = 2; // Cantidad de micrófonos
const int num_elements  = 1800; // Cantidad de grados a checar
double *angles;
double **music_spectrum;
double **X;

// FFTW buffers
double complex *in_fft, *in_time, *out_fft, *out_time;
// Solo definimos un plan para reciclar
fftw_plan fft_forward, fft_backward;

// JACK
jack_port_t *input_port1, *input_port2;
jack_client_t *client;

// Frecuencia de muestreo
double sample_rate;
// Tamaño del buffer
int buffer_size;
int buffer_index = 0;

// Tamaño del buffer de FFT
int fft_buffer_size;

// Buffers de JACK
jack_default_audio_sample_t *mic1, *mic2;

///////////// Funciones ////////////////
void set_angles(void) {
  // Define los ángulos a buscar dentro de MUSIC
  double start  = -90.0;
  double finish = 90.0;

  // Reservamos el espacio
  angles = (double *)malloc(num_elements * sizeof(double));
  if (angles == NULL) {
    printf('Fallo en alocacion de angles\n');
  }

  double increment = (finish - start) / (num_elements);
  // Llenamos el arreglo
  for (int i = 0; i < num_elements; i+=1) {
    angles[i] = start + increment*i;
  }
}

void music_spectrum_alloc(void) {
  music_spectrum = (double **)malloc(mic_n * sizeof(double*));
  if (music_spectrum == NULL) {
    printf('Fallo en inicio de music_spectrum_alloc\n');
  }

  for (int i = 0; i < mic_n; i+=1) {
    music_spectrum[i] = (double *)malloc(num_elements * sizeof(double));
    if (music_spectrum[i] == NULL) {
      printf('Fallo en segunda parte de music_spectrum_alloc. Iter: %d\n', i);
    }

    // Rellenamos la fila con 1
    for (int j = 0; j < num_elements; j+=1) {
      music_spectrum[i][j] = 1.0;
    }
  }
}


///////////// Callback /////////////////
int jack_callback(jack_nframes_t nframes, void *arg) {
  // Hacemos el arreglo X
  //  Obtenemos las ffts de las señales y llenamos el arreglo

  // Iteramos por cada frecuencia
  //  Calculamos R
  //  Eigendescomposicion de R
  //  Sort eigenvalues (descending)
  //  Sort Q eigenvectors
  

  return 0;
}
///////////// Cosas de JACK ////////////

///////////// main /////////////////////
