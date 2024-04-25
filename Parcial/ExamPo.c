/*Desfase en el dominio de la frecuencia.
Nota: El desfase se pide en unidad de tiempo pero se debe convertir a frecuencia
*/


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <jack/jack.h>

// FFTW3
#include <complex.h> //Complez siempre va primero
#include <fftw3.h>

double complex *i_fft, *i_time, *o_fft, *o_time; //variables para la transformada
fftw_plan i_forward, o_inverse;

// incluir el tipo de variable de jack

jack_port_t *input_port;
jack_port_t *output_port;
jack_port_t *output1_port;
jack_client_t *client;

//frecuencia de muestreo
double sample_rate;

//  Variables de desfase
double time_des; //seg
//char *td;    Aquí tengo un problema para usar atof.

//Buffer 3 para corroborar el desfase
int delay_samples;
jack_default_audio_sample_t *b_samples;
int b_samples_i = 0;

int fft_size; // recuerda que esto es igual 2*nframes
double *freqs, *hann, *hann_synth; //estos tiene tamaño fft_size
double synth_comp;
jack_default_audio_sample_t *b1, *b2; //esto tiene tamaño fft_size

void filter(jack_default_audio_sample_t *data){

	// Obteniendo la transformada de Fourier de este periodo
	int i;
	for(i = 0; i < fft_size; i++){
		i_time[i] = data[i]*hann_synth[i]; //aplicamos ventana de hann
	}
	fftw_execute(i_forward);

	// Aquí se hace el desfase

	o_fft[0] = i_fft[0]; //DC
	for(i = 1; i < fft_size; i++){
  		o_fft[i] = i_fft[i]*cexp(freqs[i]*(2*M_PI)*(-I)*time_des); //Ecuación de desfase

	}

	// Regresando al dominio del tiempo
	fftw_execute(o_inverse);
	for(i = 0; i < fft_size; i++){
		data[i] = creal(o_time[i])/fft_size; //fftw3 Normalizamos
		data[i] *= hann_synth[i]; // aplicamos WOLA
	}
}

int jack_callback (jack_nframes_t nframes, void *arg){
	jack_default_audio_sample_t *in, *out, *out1;
	int i;

	in = (jack_default_audio_sample_t *)jack_port_get_buffer (input_port, nframes);
	out = (jack_default_audio_sample_t *)jack_port_get_buffer (output_port, nframes);

	/* Suponemos:
	   1. b1 supongo que las ultimas 2 ventanas ya tienen Hann y estan filtradas
	   2. b2 tiene en su primera mitad la última ventana sin Hann.
		*/

	// Pasar "in" a la segunda mitad de b2
	for(i = 0; i < nframes; ++i){
	  b2[nframes+i] = in[i];
	}

	// "Hanneamos" b2
	filter(b2);

	// Sobrelape y suma para obtener "out"
	for(i = 0; i < nframes; ++i){
	  out[i] = (b1[nframes+i] + b2[i]);
	}

	// Pasamos b2 a b1 (Para poder hacer el circulito)
	for(i = 0; i < fft_size; ++i){
	  b1[i] = b2[i];
	}

	// Pasar "in" a la primera mitad de b2
	for(i = 0; i < nframes; ++i){
	  b2[i] = in[i];
	}

	//Buffer 3, para confirmar

	out1 = jack_port_get_buffer (output1_port, nframes);
	//señal con desfase
	delay_samples=time_des*sample_rate;

	for (i = 0; i < nframes; ++i){

	out1[i] = b_samples[b_samples_i];
	b_samples[b_samples_i] = in[i];
	b_samples_i++;
	if (b_samples_i >= nframes)
  	b_samples_i = 0;

}
return 0;
}

/**
 * JACK calls this shutdown_callback if the server ever shuts down or
 * decides to disconnect the client.
 */
void jack_shutdown (void *arg){
	exit (1);
}


int main (int argc, char *argv[]) {
	const char *client_name = "jack_fft_desfase";
	jack_options_t options = JackNoStartServer;
	jack_status_t status;

	printf("Enter the value in seconds of the delay: ");
	scanf ("%lf",&time_des);
	printf("%lf",time_des);

	/* open a client connection to the JACK server */
	client = jack_client_open (client_name, options, &status);
	if (client == NULL){
		/* if connection failed, say why */
		printf ("jack_client_open() failed, status = 0x%2.0x\n", status);
		if (status & JackServerFailed) {
			printf ("Unable to connect to JACK server.\n");
		}
		exit (1);
	}

	/* if connection was successful, check if the name we proposed is not in use */
	if (status & JackNameNotUnique){
		client_name = jack_get_client_name(client);
		printf ("Warning: other agent with our name is running, `%s' has been assigned to us.\n", client_name);
	}

	/* tell the JACK server to call 'jack_callback()' whenever there is work to be done. */
	jack_set_process_callback (client, jack_callback, 0);


	/* tell the JACK server to call 'jack_shutdown()' if it ever shuts down,
	   either entirely, or if it just decides to stop calling us. */
	jack_on_shutdown (client, jack_shutdown, 0);


	/* display the current sample rate. */
	printf ("Sample rate: %d\n", jack_get_sample_rate (client));
	printf ("Window size: %d\n", jack_get_buffer_size (client));
	sample_rate = (double)jack_get_sample_rate(client);
	int nframes = jack_get_buffer_size (client);
	fft_size = 2*nframes;

	//preparing FFTW3 buffers
	i_fft = (double complex *) fftw_malloc(sizeof(double complex) * fft_size);
	i_time = (double complex *) fftw_malloc(sizeof(double complex) * fft_size);
	o_fft = (double complex *) fftw_malloc(sizeof(double complex) * fft_size);
	o_time = (double complex *) fftw_malloc(sizeof(double complex) * fft_size);

	//buffer 3 para revisar
	b_samples = (jack_default_audio_sample_t *)malloc(nframes *sizeof(jack_default_audio_sample_t));


	i_forward = fftw_plan_dft_1d(fft_size, i_time, i_fft , FFTW_FORWARD, FFTW_MEASURE);
	o_inverse = fftw_plan_dft_1d(fft_size, o_fft , o_time, FFTW_BACKWARD, FFTW_MEASURE);

	int i;
	freqs = (double *) malloc(sizeof(double) * fft_size); //Esta indica en que frecuencia estoy
	freqs[0] = 0.0;
	double f1 = sample_rate/(double)fft_size;
	for (i = 1; i < nframes; i++){
	  freqs[i] = (double)i*f1;
	  freqs[fft_size-i] = -freqs[i];
	}
	freqs[nframes] = sample_rate/2;

	b1 = (jack_default_audio_sample_t *) malloc(sizeof(jack_default_audio_sample_t) * fft_size);
	b2 = (jack_default_audio_sample_t *) malloc(sizeof(jack_default_audio_sample_t) * fft_size);
	hann = (double *) malloc(sizeof(double) * fft_size);
	hann_synth = (double *) malloc(sizeof(double) * fft_size); //ventana de sintesis
	for (i = 0; i < fft_size; i++){
	  hann[i] = 0.5*(1 - cos((2*M_PI*i)/(fft_size)));
	  hann_synth[i] = sqrt(hann[i]);
	  synth_comp += hann[i]*hann_synth[i];
	  b1[i] = 0.0;	//iniciamos b1
	  b2[i] = 0.0;	//iniciamos b2
	}
	synth_comp = fft_size/synth_comp;

	/* create the agent input port */
	input_port = jack_port_register (client, "input", JACK_DEFAULT_AUDIO_TYPE,JackPortIsInput, 0);

	/* create the agent output port */
	output_port = jack_port_register (client, "output",JACK_DEFAULT_AUDIO_TYPE,JackPortIsOutput, 0);
	output1_port = jack_port_register (client, "output_1",JACK_DEFAULT_AUDIO_TYPE,JackPortIsOutput, 0);

	/* check that both ports were created succesfully */
	if ((input_port == NULL) || (output_port == NULL)) {
		printf("Could not create agent ports. Have we reached the maximum amount of JACK agent ports?\n");
		exit (1);
	}
	//Salida adicional de desfase de muestras
	if ((input_port == NULL) || (output1_port == NULL)) {
		printf("Could not create agent ports. Have we reached the maximum amount of JACK agent ports?\n");
		exit (1);
	}
	/* Tell the JACK server that we are ready to roll.
	   Our jack_callback() callback will start running now. */
	if (jack_activate (client)) {
		printf ("Cannot activate client.");
		exit (1);
	}

	printf ("Agent activated.\n");

	/* Connect the ports.  You can't do this before the client is
	 * activated, because we can't make connections to clients
	 * that aren't running.  Note the confusing (but necessary)
	 * orientation of the driver backend ports: playback ports are
	 * "input" to the backend, and capture ports are "output" from
	 * it.
	 */
	printf ("Connecting ports... ");

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

	if (jack_connect (client, jack_port_name (output1_port), serverports_names[0])) {
		printf ("Fallo la conexión de la segunda Salida.\n");
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
}



