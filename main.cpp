#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <vector>

struct Posicion {
  int x;
  int y;
};

const char *his = getenv("HYPRLAND_INSTANCE_SIGNATURE");
const char *xdg = getenv("XDG_RUNTIME_DIR");

float arc_tan(float y, float x);
void aumentar_tamano();
void disminuir_tamano();
float calcular_velocidad(const Posicion &a, const Posicion &b, float tiempo);

int main() {

  // const char *his = getenv("HYPRLAND_INSTANCE_SIGNATURE");
    if (!his || !xdg) {
        std::cerr << "Error: Variables de entorno no definidas." << std::endl;
        return 1;
    }
  // vector de posiciones con limite de 10
  std::vector<Posicion> posiciones;
  posiciones.reserve(10);
  int veces = 0,cambios_seguidos = 0, time_to_wait = 0;

  std::string socketPath = std::string(xdg) + "/hypr/" + his + "/.socket.sock";
  std::string comando = "cursorpos";

  while (true) { // Loop infinito
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
      perror("Error al crear el socket");
      return 1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
      perror("Error al conectar con el socket");
      close(sockfd);
      return 1;
    }

    ssize_t bytesEnviados = write(sockfd, comando.c_str(), comando.size());
    if (bytesEnviados < 0) {
      perror("Error al enviar el comando");
      close(sockfd);
      return 1;
    }

    shutdown(sockfd, SHUT_WR);

    char buffer[256];
    ssize_t bytesLeidos = read(sockfd, buffer, sizeof(buffer) - 1);
    if (bytesLeidos < 0) {
      perror("Error al leer la respuesta");
      close(sockfd);
      return 1;
    }

    close(sockfd);

    buffer[bytesLeidos] = '\0';
    // std::cout << "Posición del cursor: " << buffer << std::endl;
    //  Parsear la respuesta, se delimita por una coma
    int x, y;
    if (sscanf(buffer, "%d,%d", &x, &y) != 2) {
        std::cerr << "Error al parsear la posición del cursor." << std::endl;
        continue;
    }
    Posicion pos = {x, y};
    posiciones.push_back(pos);
    int tiempo = 10; // tiempo de espera en ms
    // velocidad de el movimiento del cursor entre los dos penultimos puntos
    // (posciciones 8 y 9)
    float velocidad_pen =calcular_velocidad(posiciones[8], posiciones[7], tiempo);
    float velocidad_ult = calcular_velocidad(posiciones[9], posiciones[8], tiempo);

    if (std::abs(velocidad_pen - velocidad_ult) > 5) {
      //std::cout << "Velocidad de movimiento del cursor: " << velocidad_ult
      //          << " px/s" << std::endl;
      veces++;
    } else {
      veces = 0;
    }

    if (posiciones.size() > 10) {
      posiciones.erase(posiciones.begin());
    }

    if (veces == 3) {
      veces = 0;
      cambios_seguidos++;
    }

    if (cambios_seguidos == 3) {
      // medir distanciae entre el primer y utlimo punto
      float distancia = sqrt(pow(posiciones[0].x - posiciones[9].x, 2) +
                             pow(posiciones[0].y - posiciones[9].y, 2));
      std::cout << "Distancia recorrida: " << distancia << " px" << std::endl;
      if (distancia < 250) {
        std::cout << "Cambio de tamaño" << std::endl;
        cambios_seguidos = 0;
        time_to_wait = 30;
        aumentar_tamano();
      }
    }

    if (time_to_wait > 0) {
      time_to_wait--;
    }

    if (time_to_wait == 1) {
      std::cout << "Volver a tamaño original" << std::endl;
      time_to_wait--;
      disminuir_tamano();
    }

    usleep(50000); // Esperar 50ms
    // imprimir todas las variables para hacer debug
    // std::cout << "Velocidad de movimiento del cursor: " << velocidad_ult
    //          << " px/s" << std::endl;
    // std::cout << "Angulo de movimiento del cursor: " << angulo_ult << "°"
    //          << std::endl;
    // std::cout << "Esperando: " << time_to_wait << std::endl;
    // std::cout << "Veces: " << veces << std::endl;
    // std::cout << "Cambios seguidos: " << cambios_seguidos << std::endl;
    // std::cout << "--------------------------------" << std::endl;
  }

  return 0;
}


void aumentar_tamano() {
  std::string comando = "setcursor default 50";
  std::string socketPath = std::string(xdg) + "/hypr/" + his + "/.socket.sock";
  int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("Error al crear el socket");
    exit(1);
  }

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);

  if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("Error al conectar con el socket");
    close(sockfd);
    exit(1);
  }

  ssize_t bytesEnviados = write(sockfd, comando.c_str(), comando.size());
  if (bytesEnviados < 0) {
    perror("Error al enviar el comando");
    close(sockfd);
    exit(1);
  }

  shutdown(sockfd, SHUT_WR);

  close(sockfd);
}

void disminuir_tamano() {
  std::string comando = "setcursor default 25";
  std::string socketPath = std::string(xdg) + "/hypr/" + his + "/.socket.sock";
  int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("Error al crear el socket");
    exit(1);
  }

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);

  if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("Error al conectar con el socket");
    close(sockfd);
    exit(1);
  }

  ssize_t bytesEnviados = write(sockfd, comando.c_str(), comando.size());
  if (bytesEnviados < 0) {
    perror("Error al enviar el comando");
    close(sockfd);
    exit(1);
  }

  shutdown(sockfd, SHUT_WR);

  close(sockfd);
}

float calcular_velocidad(const Posicion &a, const Posicion &b, float tiempo) {
    return sqrt(pow(b.x - a.x, 2) + pow(b.y - a.y, 2)) / tiempo;
}
