#include "config.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <vector>

struct Posicion {
  int x;
  int y;
};

enum class EstadoCliente { OCULTO = 0, VISIBLE = 1, FULLSCREEN = 2 };

const char *his = getenv("HYPRLAND_INSTANCE_SIGNATURE");
const char *xdg = getenv("XDG_RUNTIME_DIR");

bool enviar_comando_socket(const std::string &comando, std::string &respuesta) {
  if (!his || !xdg) {
    std::cerr << "Error: Variables de entorno no definidas." << std::endl;
    return false;
  }

  std::string socketPath = std::string(xdg) + "/hypr/" + his + "/.socket.sock";
  int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("Error al crear el socket");
    return false;
  }

  struct sockaddr_un addr;
  std::memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  std::strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);

  if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("Error al conectar con el socket");
    close(sockfd);
    return false;
  }

  ssize_t bytesEnviados = write(sockfd, comando.c_str(), comando.size());
  if (bytesEnviados < 0) {
    perror("Error al enviar el comando");
    close(sockfd);
    return false;
  }

  shutdown(sockfd, SHUT_WR); // No más datos para enviar

  // Leer toda la respuesta
  char buffer[256];
  respuesta.clear();
  ssize_t bytesLeidos;
  while ((bytesLeidos = read(sockfd, buffer, sizeof(buffer) - 1)) > 0) {
    buffer[bytesLeidos] = '\0';
    respuesta += buffer;
  }

  if (bytesLeidos < 0) {
    perror("Error al leer la respuesta");
    close(sockfd);
    return false;
  }

  close(sockfd);
  return true;
}

float calcular_velocidad(const Posicion &a, const Posicion &b, float tiempo) {
  return std::sqrt(std::pow(b.x - a.x, 2) + std::pow(b.y - a.y, 2)) / tiempo;
}

bool obtener_posicion_cursor(Posicion &pos) {
  std::string respuesta;
  if (!enviar_comando_socket("cursorpos", respuesta)) {
    return false;
  }
  if (sscanf(respuesta.c_str(), "%d,%d", &pos.x, &pos.y) != 2) {
    std::cerr << "Error al parsear la posición del cursor." << std::endl;
    return false;
  }
  return true;
}

void cambiar_tamano_cursor(int tamaño) {
  std::string comando = "setcursor default " + std::to_string(tamaño);
  std::string respuesta;
  if (!enviar_comando_socket(comando, respuesta)) {
    std::cerr << "Error al modificar el tamaño del cursor." << std::endl;
    exit(1);
  }
}

void aumentar_tamano() {
  cambiar_tamano_cursor(50);
} // 70 es mejor pero Hyprland se laggea
void disminuir_tamano() { cambiar_tamano_cursor(25); }

void ejecutar_comando(const std::string &cmd) { system(cmd.c_str()); }

void mostrar_dock() { ejecutar_comando("pkill -36 -f nwg-dock-hyprland"); }

void ocultar_dock() { ejecutar_comando("pkill -37 -f nwg-dock-hyprland"); }

void lanzar_dock_inicial() {
  std::string flags =
      " -r -i 64 -w 10 -mb 6 -hd 0 -c 'qs -p "
      "/home/plof/.config/quickshell/app_launcher/main.qml' -ico "
      "'/usr/share/icons/kora/actions/symbolic/view-app-grid-symbolic.svg'";
  std::string cmd = "nwg-dock-hyprland" + flags;
  ejecutar_comando(cmd);
  // std::this_thread::sleep_for(std::chrono::milliseconds(600));
}

std::string ejecutar_y_obtener_salida(const std::string &cmd) {
  std::string salida;
  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe)
    return "";
  char buffer[256];
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    salida += buffer;
  }
  pclose(pipe);
  return salida;
}

EstadoCliente evaluarDock(int monitor_height, int dock_height) {
  try {
    // Obtener información de monitores
    std::string monitors_json =
        ejecutar_y_obtener_salida("hyprctl monitors -j");
    auto monitors = nlohmann::json::parse(monitors_json);

    if (!monitors.is_array() || monitors.empty()) {
      std::cerr << "Formato de monitores inválido" << std::endl;
      return EstadoCliente::OCULTO;
    }

    // Buscar el monitor enfocado
    auto focused_monitor =
        std::find_if(monitors.begin(), monitors.end(),
                     [](const auto &m) { return m.value("focused", false); });

    if (focused_monitor == monitors.end()) {
      std::cerr << "No se encontró monitor enfocado" << std::endl;
      return EstadoCliente::OCULTO;
    }

    int active_ws = focused_monitor->value("activeWorkspace",
                                           nlohmann::json({{"id", 0}}))["id"];
    int special_ws = focused_monitor->value("specialWorkspace",
                                            nlohmann::json({{"id", 0}}))["id"];
    int ws_id = (special_ws == 0) ? active_ws : special_ws; // workspace activo

    // Obtener workspaces
    std::string workspaces_json =
        ejecutar_y_obtener_salida("hyprctl workspaces -j");
    auto workspaces = nlohmann::json::parse(workspaces_json);

    if (!workspaces.is_array()) {
      std::cerr << "Formato de workspaces inválido" << std::endl;
      return EstadoCliente::OCULTO;
    }

    // Buscar el workspace actual
    auto workspace = std::find_if(
        workspaces.begin(), workspaces.end(),
        [ws_id](const auto &ws) { return ws.value("id", -1) == ws_id; });

    int window_count = 0;
    if (workspace != workspaces.end()) {
      window_count = workspace->value("windows", 0);
    }

    if (window_count == 0)
      return EstadoCliente::VISIBLE;

    // Obtener clientes
    std::string clients_json = ejecutar_y_obtener_salida("hyprctl clients -j");
    auto clients = nlohmann::json::parse(clients_json);

    if (!clients.is_array()) {
      std::cerr << "Formato de clientes inválido" << std::endl;
      return EstadoCliente::OCULTO;
    }

    auto shouldShow = EstadoCliente::VISIBLE;
    for (const auto &client : clients) {
      if (client.value("workspace", nlohmann::json({{"id", -1}}))["id"] !=
          ws_id) {
        continue;
      }

      auto at = client.value("at", nlohmann::json::array({0, 0}));
      auto size = client.value("size", nlohmann::json::array({0, 0}));

      // Evaluar si el cliente actual esta en pantalla completa no se va a
      // mostrar el dock

      if (client.value("fullscreen", -1) != 0) {
        // std::cout << "Cliente en pantalla completa" << std::endl;
        // std::cout << "Clientes: " << client.dump() << std::endl;
        shouldShow = EstadoCliente::FULLSCREEN;
        break;
      }

      int posY = at[1].is_number() ? at[1].get<int>() : 0;
      int sizeY = size[1].is_number() ? size[1].get<int>() : 0;

      int free_space = monitor_height - posY - sizeY;
      if (free_space < dock_height) {
        shouldShow = EstadoCliente::OCULTO;
        break;
      }
    }

    return shouldShow;

  } catch (const nlohmann::json::exception &e) {
    std::cerr << "Error de JSON: " << e.what() << std::endl;
    return EstadoCliente::OCULTO;
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return EstadoCliente::OCULTO;
  }
}

bool obtener_info_monitor(int &width, int &height) {
  FILE *pipe = popen("hyprctl monitors -j", "r");
  if (!pipe) {
    std::cerr << "Error al ejecutar hyprctl monitors" << std::endl;
    return false;
  }

  std::string result;
  char buffer[256];
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    result += buffer;
  }
  pclose(pipe);

  try {
    auto monitors = nlohmann::json::parse(result);

    if (!monitors.is_array() || monitors.empty()) {
      std::cerr << "Formato de monitores inválido" << std::endl;
      return false;
    }

    auto &primer_monitor = monitors[0];

    if (!primer_monitor.contains("width") ||
        !primer_monitor["width"].is_number()) {
      std::cerr << "No se pudo obtener el width del monitor" << std::endl;
      return false;
    }

    if (!primer_monitor.contains("height") ||
        !primer_monitor["height"].is_number()) {
      std::cerr << "No se pudo obtener el height del monitor" << std::endl;
      return false;
    }

    width = primer_monitor["width"];
    height = primer_monitor["height"];

  } catch (const nlohmann::json::exception &e) {
    std::cerr << "Error al parsear JSON: " << e.what() << std::endl;
    return false;
  }

  return true;
}

int main() {
  if (!his || !xdg) {
    std::cerr << "Error: Variables de entorno no definidas." << std::endl;
    return 1;
  }

  int mon_width = 0, mon_height = 0;
  if (!obtener_info_monitor(mon_width, mon_height)) {
    return 1;
  }
  int min_w = mon_width / 2 - 400;
  int max_w = mon_width / 2 + 400;
  int min_y = mon_height * 90 / 100; // zona inferior del monitor

  lanzar_dock_inicial();
  bool dockVisible = true; // estado actual del dock

  std::vector<Posicion> posiciones;
  posiciones.reserve(10);
  int veces = 0, cambios_seguidos = 0;
  int time_to_wait = 0; // contador para revertir el tamaño del cursor

  while (true) {
    Posicion pos;
    if (!obtener_posicion_cursor(pos)) {
      // std::this_thread::sleep_for(std::chrono::milliseconds(50));
      continue;
    }
    posiciones.push_back(pos);
    if (posiciones.size() > 10) {
      posiciones.erase(posiciones.begin());
    }
    if (posiciones.size() >= 10) {
      float velocidad_pen =
          calcular_velocidad(posiciones[8], posiciones[7], SENSIBILITY);
      float velocidad_ult =
          calcular_velocidad(posiciones[9], posiciones[8], SENSIBILITY);
      if (std::abs(velocidad_pen - velocidad_ult) > 4) {
        // std::cout << "Una vez" << std::endl;
        veces += 100;
      } else {
        veces--; // Disminuir el contador pero no 0
      }
      if (veces >= 200) {
        veces = 0;
        cambios_seguidos++;
        // std::cout << "Una cambio seguido" << std::endl;
      }
      if (cambios_seguidos >= 2) {
        cambios_seguidos = 0;
        float distancia =
            std::sqrt(std::pow(posiciones[0].x - posiciones[9].x, 2) +
                      std::pow(posiciones[0].y - posiciones[9].y, 2));
        std::cout << "Distancia recorrida: " << distancia << " px" << std::endl;
        if (distancia < DISTANCE_SENSIBILITY) {
          // std::cout << "Cambio de tamaño del cursor" << std::endl;
          time_to_wait = TIME_TO_REVERT; // Tiempo de espera para revertir
          aumentar_tamano();
        }
      }
    }
    if (time_to_wait > 0) {
      time_to_wait--;
      if (time_to_wait == 1) {
        // std::cout << "Restaurando tamaño original del cursor" << std::endl;
        disminuir_tamano();
      }
    }

    bool cursorZona = (pos.y > min_y && pos.x >= min_w && pos.x <= max_w);
    auto dockWorkspace = evaluarDock(mon_height, DOCK_HEIGHT);
    bool shouldShowDock = cursorZona || dockWorkspace == EstadoCliente::VISIBLE;
    if (dockWorkspace == EstadoCliente::FULLSCREEN) {
      shouldShowDock = false;
    }
    if (shouldShowDock && !dockVisible) {
      // std::cout << "Mostrando dock" << std::endl;
      mostrar_dock();
      dockVisible = true;
    } else if (!shouldShowDock && dockVisible) {
      // std::cout << "Ocultando dock" << std::endl;
      ocultar_dock();
      dockVisible = false;
    }

    usleep(1000 * 20);
  }

  return 0;
}
