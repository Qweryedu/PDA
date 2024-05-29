/*
Implementación de MUSIC multi fuente como la primera parte 
 del proyecto final
Eduardo García Alarcón 05/2024
Procesamiento Digital de Audio
PCIC - IIMAS
*/

/// Bibliotecas de C
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
// JACK
#include <jack/jack.h>
// FFTW3
#include <fftw3.h>

/// Bibliotecas de C++
#include <complex>
#include <Eigen/Dense>
#include <vector>
#include <numeric>
#include <iostream>


using namespace std;
// using namespace Eigen;

///////////// Varibales globales ///////
const double vel_sonido = 343;   // metros/segundos
const double dist_mic   = 0.20;  // Distancia entre micrófonos, checar AIRA
const int numMic        = 2; // Cantidad de micrófonos
const int numSig        = 1; // Cantidad de señales
const int num_elements  = 1800; // Cantidad de grados a checar
std::vector<double> angles; // Angles vector
int numAngles;
Eigen::MatrixXcd X; // Data matrix
std::complex <double> *R;
Eigen::VectorXd w; // Frequency vector
int min_freq =40;
int max_freq = 40000;
int *search_freq;
int freq_range;
Eigen::MatrixXcd this_X;
double *ventanaHann;

// FFTW buffers
std::complex <double> *in_fft, *in_time, *out_fft, *out_time;
// Solo definimos un plan para reciclar
fftw_plan fft_forward, fft_backward;

// JACK
jack_port_t *input_port1, *input_port2;
jack_client_t *client;

// Frecuencia de muestreo
int sample_rate;
// Tamaño del buffer
int buffer_size;
int buffer_index = 0;

// Tamaño del buffer de FFT
int fft_buffer_size;

// Buffers de JACK
jack_default_audio_sample_t *mic1, *mic2;

///////////// Funciones ////////////////
void set_angles(void) {
  // // Define los ángulos a buscar dentro de MUSIC

  // Intentaremos checar todos los ángulos por ahora
  // Se checan los ángulos con resolución de 1 porque los humanos tenemos res de 3° aproximadamente. 
  //Llenamos el vector
  for (int angulo = -90; angulo<90; angulo+=1) {
    angles.push_back(angulo);
  }
  numAngles = angles.size();
}

void set_search_freq(void) {
  // Asignamos la memoria
  search_freq = (int *)malloc(sizeof(int) * buffer_size);
  // Definir los candidatos
  // Checar con todos, checar con 40 a 40k Hz
  for (int i = 0; i < fft_buffer_size; i+=1) {
    search_freq[i] = w[i];
  }
}


void fill_w(void) {
  // Definimos el arreglo de frecuencias
  w.resize(fft_buffer_size);
  
  w[0] = 0.0;
  w[fft_buffer_size/2] = sample_rate/2;
  double w_diff = sample_rate/fft_buffer_size;
  for(int i =1; i < fft_buffer_size/2; ++i) {
	  w[i] = w_diff*i;
	  w[fft_buffer_size-i] = -w[i];
  }
}

void set_hann(void) {
  // Empezamos la ventana de Hann
  ventanaHann = (double *) malloc(sizeof(double) * fft_buffer_size);

  for (int i = 0; i < fft_buffer_size; ++i) {
    // Como es hann completo, quitamos la raiz
    ventanaHann[i] = 0.5 * ( 1 - cos(2 * M_PI * i / (fft_buffer_size)));
    // printf("%d -> %f\n", i, ventanaHann[i]);
  }
}

// Function to compute the covariance matrix
Eigen::MatrixXcd computeCovarianceMatrix(const Eigen::MatrixXcd& X) {
    return (X * X.adjoint());
}

// Function to perform eigenvalue decomposition and separate signal and noise subspaces
void eigenDecomposition(const Eigen::MatrixXcd& R, Eigen::MatrixXcd& Qs, Eigen::MatrixXcd& Qn) {
  // Iniciamos un solver del tipo de matriz Adjoint
  Eigen::SelfAdjointEigenSolver<Eigen::MatrixXcd> es(R);
  // Sacamos eigenVectores
  Eigen::MatrixXcd eigenVectors = es.eigenvectors();
  // Sacamos eigenValores
  Eigen::VectorXd eigenValues = es.eigenvalues();

  // Sort de los eigenValore para separar los eigenVectores
  std::vector<int> index(eigenValues.size());
  // Iniciamos los valores
  std::iota(index.begin(), index.end(), 0);
  // Los acomodamos de acuerdo a los eigenValores
  std::sort(index.begin(), index.end(), [&eigenValues](int i1, int i2) { return eigenValues[i1] > eigenValues[i2]; });

  // Reroganizamos los eigenVectores
  Eigen::MatrixXcd sortedEigenVectors(eigenVectors.rows(), eigenVectors.cols());
  for (int i = 0; i < index.size(); ++i) {
      sortedEigenVectors.col(i) = eigenVectors.col(index[i]);
  }

  // Sacamos los eigenVectores de señal
  Qs = sortedEigenVectors.leftCols(numSig);
  Qn = sortedEigenVectors.rightCols(eigenVectors.cols() - numSig);
}

// Function to compute the array response vector for a given angle
Eigen::VectorXcd arrayResponseVector(int numSensors, double angle, double d, double lambda) {
    Eigen::VectorXcd a(numSensors);
    std::complex<double> j(0, 1);
    for (int i = 0; i < numSensors; ++i) {
        a[i] = std::exp(j * 2.0 * M_PI * (d) * static_cast<double>(i) * std::sin(angle) / lambda);
    }
    return a;
}

// Function to compute the MUSIC pseudospectrum
Eigen::VectorXd musicSpectrum(const Eigen::MatrixXcd& signalSubspace, int numSensors, int numAngles, double d, double lambda) {
    Eigen::VectorXd spectrum(numAngles);
    Eigen::MatrixXcd proj = signalSubspace * signalSubspace.adjoint();
    for (int i = 0; i < numAngles; ++i) {
        double angle = -M_PI / 2 + i * M_PI / numAngles; // Angle range from -90 to 90 degrees
        Eigen::VectorXcd a = arrayResponseVector(numSensors, angle, d, lambda);
        std::complex<double> denominator = a.adjoint() * (Eigen::MatrixXcd::Identity(numSensors, numSensors) - proj) * a;
        spectrum[i] = (1.0) / denominator.real();
    }
    return spectrum;
}

Eigen::VectorXcd musicSpectrum2(Eigen::MatrixXcd& Qn, Eigen::MatrixXcd& a) {
  Eigen::VectorXcd music_spectrum(numAngles);
  for (int k = 0; k < numAngles; ++k) {
    std::complex<double> denominador = a.col(k).adjoint() * Qn * Qn.adjoint() * a.col(k);
    music_spectrum[k] = std::abs( 1.0 / denominador ); 
  }

  return music_spectrum;
}

Eigen::MatrixXcd steeringVectors(double this_w){
  // Steering vectors como 1's por el mic de referencia
  Eigen::MatrixXcd a = Eigen::MatrixXcd::Ones(numMic, numAngles);
  // Segundo mic
  for (int i = 0; i < numAngles; ++i) {
    a(1,i) = std::exp(std::complex<double>(0, -2 * M_PI * this_w * (dist_mic/vel_sonido) * std::sin(angles[i] * M_PI/180)));
  }
  return a;
}


///////////// Callback /////////////////
int jack_callback(jack_nframes_t nframes, void *arg) {
  // printf("Inicia callback\n");fflush(stdout);
  int i,j;
  // Obtenemos las entradas
  jack_default_audio_sample_t *in1, *in2;
  in1 = (jack_default_audio_sample_t *)jack_port_get_buffer(input_port1, nframes);
  in2 = (jack_default_audio_sample_t *)jack_port_get_buffer(input_port2, nframes);

  
  // Hanneamos ambas ventanas
  for (i=0; i<nframes; ++i) {
    in1[i] = in1[i] * ventanaHann[i];
    in2[i] = in2[i] * ventanaHann[i];
  }

  // printf("Ventanas obtenidas y hanneadas\n");fflush(stdout);
  jack_default_audio_sample_t *entradas[numMic];
  entradas[0] = in1;
  entradas[1] = in2;
  // printf("entradas completas\n");
  // Calculamos sus transformadas y guardamos en X
  for (i=0; i<numMic; ++i) {
    for (j=0; j < fft_buffer_size; ++j) {
      in_time[j] = entradas[i][j];
    }
    // Calculamos FFT
    fftw_execute(fft_forward);
    //Copiamos en X
    for (j=0; j<fft_buffer_size; ++j) {
      X(i,j) = in_fft[j];
    }
  }
  // printf("Accedimos y metimos a X los valores de las FFTs\n");fflush(stdout);
  // Definimos el espectro de MUSIC total
  Eigen::VectorXcd final_music_spectrum = Eigen::VectorXd::Zero(numAngles);
  // printf("Iniciamos final_music_spectrum y llenamos con 0's\n");fflush(stdout);
  //Checar desde aquí, posible error
  // Iteramos por cada frecuencia dentro de X
  for (i=0; i<fft_buffer_size/2; ++i) {
    // Sacamos el slice correspondiente a la primer frecuencia
    this_X = X.col(i);
    // Calculamos R
    Eigen::MatrixXcd R = computeCovarianceMatrix(this_X);

    //  Eigendescomposicion de R
      //  Sort eigenvalues (descending)
      //  Sort Q eigenvectors
      //  Get noise eigenvectors
      //  Compute steering vectors
    Eigen::MatrixXcd Qs;
    Eigen::MatrixXcd Qn;
    eigenDecomposition(R, Qs, Qn);
    
    // Calculamos los steering vectors para la frecuencia actual
    Eigen::MatrixXcd a = steeringVectors(w[i]);

    //  Compute MUSIC spectrum
    Eigen::VectorXcd tmp_music_spectrum(numAngles);
    tmp_music_spectrum = musicSpectrum2(Qn, a);

    final_music_spectrum += tmp_music_spectrum;
  }


  // Get argmax and print
  Eigen::Index maxIndex;
  double maxVal = final_music_spectrum.real().maxCoeff(&maxIndex);
  double maxAngle = angles[maxIndex];

  // Print
  std::cout << "DOI: " << maxAngle << "°" << std::endl;

  return 0;
}
///////////// Cosas de JACK ////////////
void jack_shutdown(void *arg) {
  exit(1);
}

///////////// main /////////////////////
int main(int argc, char *argv[]) {
  int i,j; // Iteradores
  printf("Implementación de MUISC\n");
  // Definimos el vector de ángulos
  set_angles();
  printf("set_angles done\n");
  
  // // Apartamos la memoria para el espectro de MUSIC
  // music_spectrum_alloc();
  // printf("music_spectrum_alloc done\n");

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
  sample_rate = (int) jack_get_sample_rate(client);
  int nframes = jack_get_buffer_size(client);
  buffer_size = nframes;
  printf ("Sample rate: %d\n", sample_rate);
  printf ("Window size: %d\n", nframes);
  fft_buffer_size = nframes;
  printf("FFT buffer size: %d\n", fft_buffer_size);

  // Definimos X
  X = Eigen::MatrixXcd::Zero(numMic, buffer_size);
  printf("X definido\n");

  // Definimos hann
  set_hann();
  printf("set_hann done\n");
  
  // Llenamos w
  fill_w();
  printf("fill_w done\n");  
  
  // Definimos el vector de frecuencias de búsqueda
  set_search_freq();
  printf("set_search_freq done\n");
    
  

  // Buffers temporales para guardar y hannear
  mic1 = (jack_default_audio_sample_t *)malloc (sizeof(jack_default_audio_sample_t) * fft_buffer_size);
  mic2 = (jack_default_audio_sample_t *)malloc (sizeof(jack_default_audio_sample_t) * fft_buffer_size);

  ///// FFTW3 
  // Buffers
  in_fft = (std::complex<double> *) fftw_malloc(sizeof(std::complex<double>) * fft_buffer_size);
  in_time = (std::complex<double> *) fftw_malloc(sizeof(std::complex<double>) * fft_buffer_size);
  out_fft = (std::complex<double> *) fftw_malloc(sizeof(std::complex<double>) * fft_buffer_size);
  out_time = (std::complex<double> *)fftw_malloc(sizeof(std::complex<double>) * fft_buffer_size);

  // Plans
  fft_forward  = fftw_plan_dft_1d(fft_buffer_size, reinterpret_cast<fftw_complex*>(in_time), reinterpret_cast<fftw_complex*>(in_fft), FFTW_FORWARD, FFTW_MEASURE);
  fft_backward = fftw_plan_dft_1d(fft_buffer_size, reinterpret_cast<fftw_complex*>(out_time), reinterpret_cast<fftw_complex*>(out_fft), FFTW_BACKWARD, FFTW_MEASURE);

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
