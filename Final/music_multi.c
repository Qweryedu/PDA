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
const int sig_n         = 1; // Cantidad de señales
const int num_elements  = 1800; // Cantidad de grados a checar
double *angles; // Angles vector
double **music_spectrum; // spectrum vector
double **X; // Data matrix
double *w; // Frequency vector

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
    printf("Fallo en alocacion de angles\n");
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
    printf("Fallo en inicio de music_spectrum_alloc\n");
  }

  for (int i = 0; i < mic_n; i+=1) {
    music_spectrum[i] = (double *)malloc(num_elements * sizeof(double));
    if (music_spectrum[i] == NULL) {
      printf("Fallo en segunda parte de music_spectrum_alloc. Iter: %d\n", i);
    }

    // Rellenamos la fila con 1
    for (int j = 0; j < num_elements; j+=1) {
      music_spectrum[i][j] = 1.0;
    }
  }
}



///////////// Callback /////////////////
int jack_callback(jack_nframes_t nframes, void *arg) {
  // Obtenemos las entradas
  // Calculamos sus transformadas
  // Hacemos la matriz X

  //  Obtenemos las ffts de las señales y llenamos el arreglo

  // Iteramos por cada frecuencia
  //  Calculamos R
  //  Eigendescomposicion de R
  //  Sort eigenvalues (descending)
  //  Sort Q eigenvectors
  //  Get noise eigenvectors
  //  Compute steering vectors
  //  Compute MUSIC spectrum

  // Get argmax and print


  return 0;
}
///////////// Cosas de JACK ////////////
void jack_shutdown(void *arg) {
  exit(1);
}
///////////// main /////////////////////
int main(int argc, char *argv[]) {
  int i,j; // Iteradores
  printf("Implementación de MUISC");
  // Definimos el vector de ángulos
  set_angles();
  // Apartamos la memoria para el espectro de MUSIC
  music_spectrum_alloc();

  // Cosas de JACK
  const char *client_name = "MUSIC";
  jack_options_t options = JackNoStartServer;
  jack_status_t status;

  // Conectamos con el cliente de JACK
  client = jack_client_open(client_name, options, &status);
  if (client == NULL) {
    printf("jack_client_open() failed, status = 0x%2.0x\n", status);
    if (status & JackServerFailed) {
      printf("Unable to connect to JACK server");
    }
    exit(1);
  }

  // Checamos que no se repita el nombre
  if (status & JackNameNotUnique) {
    client_name = jack_get_client_name(client);
    printf("Warning: other agent with our name is running, '%s' has been assigned to us.\n", client_name);
  }

  // Configuramos el callback de JACK
  jack_set_process_callback(client, jack_callback, 0);
  // Configuramos el shutdown
  jack_on_shutdown(client, jack_shutdown, 0);

  // Obtenemos el sample rate
  sample_rate = (double) jack_get_sample_rate(client);
  int nframes = jack_get_buffer_size(client);
  buffer_size = nframes;
  printf ("Sample rate: %f\n", sample_rate);
  printf ("Window size: %d\n", nframes);

  // Definimos X
  X[mic_n][buffer_size];

  // Definimos el tamaño del buffer de FFT
  fft_buffer_size = nframes;
  printf("FFT buffer size: %d\n", fft_buffer_size);

  // Buffers temporales para guardar y hannear
  mic1 = (jack_default_audio_sample_t *)malloc (sizeof(jack_default_audio_sample_t) * fft_buffer_size);
  mic2 = (jack_default_audio_sample_t *)malloc (sizeof(jack_default_audio_sample_t) * fft_buffer_size);

  ///// FFTW3 
  // Buffers
  in_fft = (double complex *) fftw_malloc(sizeof(double complex) * fft_buffer_size);
  in_time = (double complex *) fftw_malloc(sizeof(double complex) * fft_buffer_size);
  out_fft = (double complex *) fftw_malloc(sizeof(double complex) * fft_buffer_size);
  out_time = (double complex *) fftw_malloc(sizeof(double complex) * fft_buffer_size);

  // Plans
  fft_forward  = fftw_plan_dft_1d(fft_buffer_size, in_time, in_fft, FFTW_FORWARD, FFTW_MEASURE);
  fft_backward = fftw_plan_dft_1d(fft_buffer_size, out_time, out_fft, FFTW_BACKWARD, FFTW_MEASURE);

  // Entrada del sistema
  input_port1 = jack_port_register(client, "input1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);

  input_port2 = jack_port_register(client, "input2", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);

  // Checar que los puertos hayan sido creados correctamente
  if ( (input_port1==NULL) || (input_port2 == NULL) ) {
    printf("No se pudieron crear los agentes de puerto");
    exit(1);
  }

  // Activamos el agente JACK
  if (jack_activate(client)){
    printf("No se pudo activar el cliente");
  }

  printf("El agente está activo\n");
  printf("Conectando los puertos\n");

  /* Assign our input port to a server output port*/
  // Find possible output server port names
  const char **serverports_names;
  serverports_names = jack_get_ports (client, NULL, NULL, JackPortIsPhysical|JackPortIsOutput);
  if (serverports_names == NULL) {
    printf("No available physical capture (server output) ports.\n");
    exit (1);
  }
  // Connect the first available to our input port
  if (jack_connect (client, serverports_names[0],jack_port_name (input_port1))) {
    printf("Cannot connect input port.\n");
    exit (1);
  }
  // Connect the first available to our input port
  if (jack_connect (client, serverports_names[1],jack_port_name (input_port2))) {
    printf("Cannot connect input port.\n");
    exit (1);
  }
  // free serverports_names variable for reuse in next part of the code
  free (serverports_names);
  
  printf("done.\n");

  sleep(-1);
  
  jack_client_close(client);
  exit(0);


  return 0;
}
