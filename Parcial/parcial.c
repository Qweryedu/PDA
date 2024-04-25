/*
Examen Parcial de Procesamiento Digital de Audio Febrero 2024
Eduardo García Alarcón - IIMAS

Instrucciones:
Hacer un agente de JACK que atrasa (o desfasa) la señal de la entrada por medio del operador de la exponencial imaginaria, con un argumento en segundos

Procedimiento:
- Obtenemos los datos del buffer
  - 
- Se ventanean con raíz cuadrada de Hann en tiempo
  - Ventana con tamaño 1024
- Se convierten a frecuencia
- Se hace el procesamiento deseado
  - Se multiplica por la exponencial imaginaria
- Se ventanean con raíz cuadrada de Hann en frecuencia
- Se regresa a tiempo
- Se saca el valor por el puerto de salida
*/

//// Bibliotecas
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
// Incluimos JACK
#include <jack/jack.h>
// Incluimos FFTW header
#include <complex.h>
#include <fftw3.h>

//// Variables globales
// FFTW buffers
double complex *i_fft, *i_time, *o_fft, *o_time;
fftw_plan i_forward, o_inverse;

// JACK 
jack_port_t *input_port, *output_port, *ref_output_port;
jack_client_t *client;

// Frecuencia de muestreo 
double sample_rate;
// Desfase proporcionado por el usuario
double desfase;
// Tamaño del buffer e index
int buffer_size;
int buffer_index = 0;

// Tamaño del buffer de FFT
int fft_buffer_size;

// Se declara el buffer de Jack
jack_default_audio_sample_t *b1, *b2, *b3;

// Arreglo de las frecuencias y sus indices
double  *freq;
// Arreglo de la ventana de Hann
double *ventanaHann;

void hanneo() {
  // Recorremos todo el buffer
  for (int i = 0; i < fft_buffer_size; ++i) {
    // Calculamos el producto punto a punto entre el buffer y Hann
    b2[i] *= ventanaHann[i];
  }
}

void filter(int buffer_size){
  // Realizamos un Hanneo
  hanneo();

  // Copiamos la información a procesar en el buffer de FFTW
  for (int i = 0; i < buffer_size; ++i) {
    i_time[i] = b2[i];
  }

  // Procesamos en el dominio de la frecuencia; ejecutamos la FFT
  fftw_execute(i_forward);

  // Pasamos el componente de DC
  o_fft[0] = i_fft[0];
  // Realizamos el desplazamiento 
  for (int i = 1; i < buffer_size; ++i) {
    // multiplicamos el valor de la entrada por la exponencial imaginaria
    o_fft[i] = i_fft[i]* cexp(-I*2*M_PI*freq[i]*desfase);
  }

  // Regresamos al dominio del tiempo
  fftw_execute(o_inverse);

  // Normalizamos
  for (int i = 0; i < buffer_size; ++i) {
    b2[i] = creal(o_time[i])/buffer_size;
  }

  // Se realiza el otro Hanneo
  hanneo();

}

//// Definimos el Call Back
int jack_callback(jack_nframes_t nframes, void *arg) {
  int i;
  jack_default_audio_sample_t *in, *out, *out_ref;

  // Obtenemos los buffers
  in = jack_port_get_buffer(input_port, nframes);
  out = jack_port_get_buffer(output_port, nframes);
  out_ref = jack_port_get_buffer(ref_output_port, nframes);

  // Paso 0
  // Copiamos la primer mitad de b2 a la salida de referencia
  for ( i = 0; i < nframes; ++i) {
    out_ref[i] = b2[i];
  }

  // Primer paso 
  // copiar C en la segunda mitad de b2
  for ( i = 0; i < nframes; ++i) {
    b2[nframes + i] = in[i];
  }

  // Segundo paso
  filter(fft_buffer_size);

  // Tercer paso
  // Copiamos la salida de Fourier a b2
  // Sumamos los vectores filtrados

 // Sumamos para la salida con desfase
  for ( i = 0; i < nframes; ++i) {
    out[i] = b1[nframes + i] + b2[i];
  }

  // Cuarto paso
  // Copiamos el contenido de b2 a b1
  for ( i = 0; i < fft_buffer_size; ++i) {
    b1[i] = b2[i];
  }

  // Quinto paso
  // Copianos C a la primer mitad de b2
  for ( i = 0; i < nframes; ++i) {
    b2[i] = in[i];
  }

  return 0;
}

// Cosas de JACK
void jack_shutdown(void *arg){
  exit(1);
}

int main(int argc, char *argv[]) {
  // Verificamos que manden los argumentos correctos
  if (argc < 2) {
    printf("Necesito la cantidad en segundos a desfasar.\n");
    exit(1);
  }

  // Rescatamos al tamaño del buffer
  desfase = atof(argv[1]);

  // Verificar que sea menor a nuestra latencia
  if (desfase > .41) {
    printf("El desfase es mayor que nuestra latencia");
    exit(1);
  }

  // Cosas de JACK
  const char *client_name = "parcial";
  jack_options_t options = JackNoStartServer;
  jack_status_t status;

  // Conectamos con el cliente de JACK
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
  printf ("Sample rate: %f\n", sample_rate);
  printf ("Window size: %d\n", nframes);

  // Definimos el tamaño del buffer de FFT
  fft_buffer_size = nframes*2;
  printf("FFT buffer size: %d\n", fft_buffer_size);
  
  // Empezamos la ventana de Hann
  ventanaHann = (double *) malloc(sizeof(double) * fft_buffer_size);

  for (int i = 0; i < fft_buffer_size; ++i) {
    ventanaHann[i] = sqrt(0.5 * ( 1 - cos(2 * M_PI * i / (fft_buffer_size)) ));
    // printf("%d -> %f\n", i, ventanaHann[i]);
  }

  // Buffers temporales para guardar y hannear
  b1 = (jack_default_audio_sample_t *) malloc(sizeof(jack_default_audio_sample_t)* fft_buffer_size);
  b2 = (jack_default_audio_sample_t *) malloc(sizeof(jack_default_audio_sample_t)* fft_buffer_size);

  // Preparando los buffers de  FFTW
  i_fft = (double complex *) fftw_malloc(sizeof(double complex) * fft_buffer_size);
  i_time = (double complex *) fftw_malloc(sizeof(double complex) * fft_buffer_size);
  o_fft = (double complex *) fftw_malloc(sizeof(double complex) * fft_buffer_size);
  o_time = (double complex *) fftw_malloc(sizeof(double complex) * fft_buffer_size);
  
  i_forward = fftw_plan_dft_1d(fft_buffer_size, i_time, i_fft , FFTW_FORWARD, FFTW_MEASURE);
  o_inverse = fftw_plan_dft_1d(fft_buffer_size, o_fft , o_time, FFTW_BACKWARD, FFTW_MEASURE);

  // Definimos el arreglo de frecuencias
  freq = (double *) malloc(sizeof(double) * fft_buffer_size);
  freq[0] = 0.0;
  freq[fft_buffer_size/2] = sample_rate/2;
  double freq_diff = sample_rate/fft_buffer_size;
  for(int i =1; i < fft_buffer_size/2; ++i) {
	  freq[i] = freq_diff*i;
	  freq[fft_buffer_size-i] = -freq[i];
  }

  for (int i = 0; i < fft_buffer_size; i++) {
    printf("%d -> %f\n", i, freq[i]);
  }

  // Entrada del sistema
  input_port = jack_port_register (client, "input", JACK_DEFAULT_AUDIO_TYPE,JackPortIsInput, 0);
  
  // Salida del sistema filtrado
  output_port = jack_port_register (client, "output_desfasado",JACK_DEFAULT_AUDIO_TYPE,JackPortIsOutput, 0);

  // Salida del sistema filtrado sin procesar
  ref_output_port = jack_port_register(client, "output_referencia", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

  // Checar que los puertos hayan sido creados correctamente
  if((input_port==NULL) || (output_port==NULL) || (ref_output_port == NULL)){
    printf("No se pudieron crear los agentes de puerto\n");
    exit(1);
  }

  // Activamos el agente de JACK
  if (jack_activate(client)){
    printf("No se pudo activar el cliente\n");
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
  if (jack_connect (client, serverports_names[0], jack_port_name (input_port))) {
    printf("Cannot connect input port.\n");
    exit (1);
  }
  // free serverports_names variable for reuse in next part of the code
  free (serverports_names);
  
  
  /* Assign our output port to a server input port*/
  // Find possible input server port names
  serverports_names = jack_get_ports (client, NULL, NULL, JackPortIsPhysical|JackPortIsInput);
  if (serverports_names == NULL) {
    printf("No available physical playback (server input) ports.\n");
    exit (1);
  }
  // Connect the first available to our output port
  if (jack_connect (client, jack_port_name (output_port), serverports_names[0])) {
    printf ("Cannot connect output ports.\n");
    exit (1);
  }
  if (jack_connect (client, jack_port_name (ref_output_port), serverports_names[1])) {
    printf ("Cannot connect output ports.\n");
    exit (1);
  }
  // free serverports_names variable, we're not going to use it again
  free (serverports_names);
  
  
  printf ("done.\n");
  /* keep running until stopped by the user */
  sleep (-1);
  
  
  /* this is never reached but if the program
     had some other way to exit besides being killed,
     they would be important to call.
  */
  jack_client_close (client);
  exit (0);


  exit(0);
}