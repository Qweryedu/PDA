/*
Cálculo de dirección de arribo
Eduardo García Alarcón 04/2024
Procesamiento Digital de Audio
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

/// Varibales globales
const double vel_sonido = 343;   // metros/segundos
const double dist_mic   = 0.21; // Distancia entre mic de mi laptop
// FFTW buffers
double complex *mic1_i_fft, *mic1_i_time, *mic1_o_fft, *mic1_o_time;
double complex *mic2_i_fft, *mic2_i_time, *mic2_o_fft, *mic2_o_time;
fftw_plan mic1_forward, mic1_backward;
fftw_plan mic2_forward, mic2_backward;

//JACK
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
jack_default_audio_sample_t *mic1, *mic2, *ccv;


double promedio(jack_default_audio_sample_t buffer[]) {
  double suma = 0;
  for (int i = 0; i < buffer_size; ++i) {
    suma += buffer[i];
  }

  return (double) suma/buffer_size;
}

void restaPromedio(jack_default_audio_sample_t buffer[], double promedio){
  for (int i = 0; i < buffer_size; ++i){
    buffer[i] -= promedio;
  }
}

double norma(jack_default_audio_sample_t ventana[]) {
  // Recorremos todo el buffer
  double suma_cuadrados = 0;
  for(int i = 0; i < buffer_size; ++i) {
    suma_cuadrados += ventana[i] * ventana[i];
  }
  return sqrt(suma_cuadrados);
}

int argmax(jack_default_audio_sample_t ventana[]) {
  // Iniciamos los temporales de busqueda
  double max = ventana[0];
  int max_indx = 0;

  // Iteramos los otros valores
  for (int i = 1; i<buffer_size; ++i) {
    //Checamos si el nuevo valor es mayor que el guardado
    if (max < ventana[i]) {
      max = ventana[i];
      max_indx = i;
    }
  }
  return max_indx;
}

/// Definimos el callback
int jack_callback(jack_nframes_t nframes, void *arg){
  // printf("Inicia Callback\n"); fflush(stdout);
  int i;
  jack_default_audio_sample_t *in1, *in2;
  
  // printf("Obtenemos el buffer\n"); fflush(stdout);
  // Obtenemos los buffers
  in1 = jack_port_get_buffer(input_port1, nframes);
  in2 = jack_port_get_buffer(input_port2, nframes);

  // printf("Copiamos los buffers\n"); fflush(stdout);
  for(i = 0; i < nframes; ++i) {
    mic1[i] = in1[i];
    mic2[i] = in2[i];
  }
  // Hacemos los procedimientos necesarios
  // 1) Sacamos promedio y restamos para centrar la señal
  // printf("Sacamos el promedio\n"); fflush(stdout);
  double prom_x = promedio(mic1);
  double prom_y = promedio(mic2); 
  // printf("Restamos promedio"); fflush(stdout);
  restaPromedio(mic1, prom_x);
  restaPromedio(mic2, prom_y);
  // 2) Sacamos sus ffts
  //  Copiamos la información de los buffers a las entradas
  // printf("Copiamos para la entrada de fft en tiempo"); fflush(stdout);
  for (i = 0; i<buffer_size; ++i) {
    mic1_i_time[i] = mic1[i];
    mic2_i_time[i] = mic2[i];
  }
  //  Sacamos la transformada
  // printf("Ejecutamos las transformada"); fflush(stdout);
  fftw_execute(mic1_forward);
  fftw_execute(mic2_forward);

  // 3) Multiplicamos (producto punto) Y con el conjugado de X
  // printf("Calculamos el producto punto del conjugado"); fflush(stdout);
  for (i = 0; i < buffer_size; ++i) {
    // El no conjugado es el mic de referencia (en este caso el mic1)
    mic2_o_fft[i] = conj(mic2_i_fft[i]) * mic1_i_fft[i];
  }
  //  Regresamos a tiempo
  // printf("Refresamos a tiempo"); fflush(stdout);
  fftw_execute(mic2_backward);

  // 4) Dividimos entre el producto de las normas para normalizar
  // printf("Sacamos la norma\n"); fflush(stdout);
  double mic1_norma = norma(mic1);
  double mic2_norma = norma(mic2);
  //  Dividimos todo el vector de salida
  // printf("Normalizamos\n"); fflush(stdout);
  for (i = 0; i < buffer_size; ++i){
    ccv[i] = mic2_o_time[i] / (mic1_norma * mic2_norma); 
  }
  // 5) Sacamos el argmax para conocer el desfase
  // printf("Encontramos el argmax"); fflush(stdout);
  int desfase = argmax(ccv);
  // Normalizamos el índice del desfase
  if (desfase < buffer_size/2) {
    desfase = desfase;
  }
  else {
    desfase = desfase - buffer_size;
  }
  // printf("El desfase máximo está en el índice %d\n", desfase); fflush(stdout);
  // El desfase está en muestras, por lo que necesitamos pasarlo 
  // a segundos, para posteriormente calcular el ángulo
  double desfase_tiempo = desfase / sample_rate; // muestras/(muestras/seg)=seg

  // Siguiendo la diapositiva 22 de la clase 6.1 DOA
  double angulo;

  angulo = asin(vel_sonido*desfase_tiempo/dist_mic)*(180/M_PI);
  printf("El ángulo de arribo es %f\n", angulo);

  return 0;

}

// Cosas de JACK
void jack_shutdown(void *arg){
  exit(1);
}

int main(int argc, char *argv[]) {
  printf("Programa para encontrar la dirección de arribo de una señal\n");

  // Cosas de JACK
  const char *client_name = "DirArribo";
  jack_options_t options = JackNoStartServer;
  jack_status_t status;

  // Conectamos con el cliente JACK
  client = jack_client_open(client_name, options, &status);
  if (client == NULL) {
    printf("jack_client_open() failed, status = 0x%2.0x\n", status);
    if (status & JackServerFailed) {
      printf("Unable to connect to  JACK server");
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

  // Definimos el tamaño del buffer de FFT
  fft_buffer_size = nframes;
  printf("FFT buffer size: %d\n", fft_buffer_size);
  
  

  // Buffers temporales para guardar y hannear
  mic1 = (jack_default_audio_sample_t *)malloc (sizeof(jack_default_audio_sample_t) * fft_buffer_size);
  mic2 = (jack_default_audio_sample_t *)malloc (sizeof(jack_default_audio_sample_t) * fft_buffer_size);
  ccv  = (jack_default_audio_sample_t *)malloc (sizeof(jack_default_audio_sample_t) * fft_buffer_size);
  
  // Preparando los buffers de  FFTW
  mic1_i_fft = (double complex *) fftw_malloc(sizeof(double complex) * fft_buffer_size);
  mic1_i_time = (double complex *) fftw_malloc(sizeof(double complex) * fft_buffer_size);
  mic1_o_fft = (double complex *) fftw_malloc(sizeof(double complex) * fft_buffer_size);
  mic1_o_time = (double complex *) fftw_malloc(sizeof(double complex) * fft_buffer_size);

  mic2_i_fft = (double complex *) fftw_malloc(sizeof(double complex) * fft_buffer_size);
  mic2_i_time = (double complex *) fftw_malloc(sizeof(double complex) * fft_buffer_size);
  mic2_o_fft = (double complex *) fftw_malloc(sizeof(double complex) * fft_buffer_size);
  mic2_o_time = (double complex *) fftw_malloc(sizeof(double complex) * fft_buffer_size);
  
  mic1_forward = fftw_plan_dft_1d(fft_buffer_size, mic1_i_time, mic1_i_fft , FFTW_FORWARD, FFTW_MEASURE);
  mic1_backward = fftw_plan_dft_1d(fft_buffer_size, mic1_o_fft , mic1_o_time, FFTW_BACKWARD, FFTW_MEASURE);

  mic2_forward = fftw_plan_dft_1d(fft_buffer_size, mic2_i_time, mic2_i_fft , FFTW_FORWARD, FFTW_MEASURE);
  mic2_backward = fftw_plan_dft_1d(fft_buffer_size, mic2_o_fft , mic2_o_time, FFTW_BACKWARD, FFTW_MEASURE);


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
  // // free serverports_names variable for reuse in next part of the code
  // free (serverports_names);

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

  return(0);

}