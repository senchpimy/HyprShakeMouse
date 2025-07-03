#include "config.h"
// #define DETECT_SHAKE
#ifdef DETECT_SHAKE
#include "shakedetector.h"
#endif
#include "tabletmode.h"

#include <chrono>
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

#include <errno.h>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <linux/input.h>

namespace fs = std::filesystem;

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

  shutdown(sockfd, SHUT_WR);
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
  }
}

void aumentar_tamano() { cambiar_tamano_cursor(50); }
void disminuir_tamano() { cambiar_tamano_cursor(25); }

void ejecutar_comando(const std::string &cmd) { system(cmd.c_str()); }

void mostrar_dock() { ejecutar_comando("pkill -36 -f nwg-dock-hyprland"); }
void ocultar_dock() { ejecutar_comando("pkill -37 -f nwg-dock-hyprland"); }

void lanzar_dock_inicial() {
  std::string flags =
      " -r -i 64 -w 10 -mb 6 -hd 0 -c 'qs -p "
      "/home/plof/.config/quickshell/app_launcher/main.qml' -ico "
      "'/usr/share/icons/kora/actions/symbolic/view-app-grid-symbolic.svg'";
  std::string cmd = "nwg-dock-hyprland" + flags + " &";
  ejecutar_comando(cmd);
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
    std::string monitors_json =
        ejecutar_y_obtener_salida("hyprctl monitors -j");
    auto monitors = nlohmann::json::parse(monitors_json);
    if (!monitors.is_array() || monitors.empty()) {
      std::cerr << "Formato de monitores inválido" << std::endl;
      return EstadoCliente::OCULTO;
    }

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
    int ws_id = (special_ws == 0) ? active_ws : special_ws;
    std::string workspaces_json =
        ejecutar_y_obtener_salida("hyprctl workspaces -j");
    auto workspaces = nlohmann::json::parse(workspaces_json);
    if (!workspaces.is_array()) {
      std::cerr << "Formato de workspaces inválido" << std::endl;
      return EstadoCliente::OCULTO;
    }

    auto workspace = std::find_if(
        workspaces.begin(), workspaces.end(),
        [ws_id](const auto &ws) { return ws.value("id", -1) == ws_id; });
    int window_count = 0;
    if (workspace != workspaces.end()) {
      window_count = workspace->value("windows", 0);
    }
    if (window_count == 0)
      return EstadoCliente::VISIBLE;
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

      if (client.value("fullscreen", -1) != 0) {
        shouldShow = EstadoCliente::FULLSCREEN;
        break;
      }

      auto at = client.value("at", nlohmann::json::array({0, 0}));
      auto size = client.value("size", nlohmann::json::array({0, 0}));
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

class TabletMode {
public:
  TabletMode();
  bool is_active();
  bool conect(std::string);
  bool isValid();
  ~TabletMode() {
    if (fd >= 0)
      close(fd);
  }

private:
  int fd = -1;
  int retry = 5000;
  std::string detect_device();
  bool status = false;
  void manage_iio_process();
  void setTabletMode(bool) {} // Placeholder
};

bool TabletMode::isValid() {
  if (fd >= 0)
    return true;
  retry--;
  if (retry > 0)
    return false;
  retry = 5000;
  auto device_path = detect_device();
  if (device_path.empty() || !conect(device_path))
    return false;
  return true;
}

void TabletMode::manage_iio_process() {
  if (this->status) {
    std::cout << "Modo tablet activado. Iniciando iio-hyprland..." << std::endl;
    ejecutar_comando("iio-hyprland &");
  } else {
    std::cout << "Modo tablet desactivado. Terminando iio-hyprland..."
              << std::endl;
    ejecutar_comando("pkill -f iio-hyprland");
  }
}

TabletMode::TabletMode() {
  std::string device_path = detect_device();
  if (device_path.empty()) {
    std::cerr << "No se encontró un dispositivo con SW_TABLET_MODE.\n";
    this->status = false;
  } else {
    conect(device_path);
  }
  manage_iio_process();
}

bool TabletMode::conect(std::string device_path) {
  fd = open(device_path.c_str(), O_RDONLY | O_NONBLOCK);
  if (fd < 0) {
    std::cerr << "Error abriendo el dispositivo " << device_path << ": "
              << strerror(errno) << std::endl;
    this->status = false;
    return false;
  }
  std::cout << "Dispositivo de modo tablet: " << device_path << std::endl;
  unsigned char sw_state_initial[(SW_MAX + 7) / 8];
  memset(sw_state_initial, 0, sizeof(sw_state_initial));
  if (ioctl(this->fd, EVIOCGSW(sizeof(sw_state_initial)), sw_state_initial) >=
      0) {
    this->status =
        (sw_state_initial[SW_TABLET_MODE / 8] >> (SW_TABLET_MODE % 8)) & 1;
    std::cout << "Estado inicial de tablet mode: "
              << (this->status ? "ACTIVADO" : "DESACTIVADO") << std::endl;
  } else {
    std::cerr << "Constructor TabletMode: Error con ioctl EVIOCGSW: "
              << strerror(errno) << ". Asumiendo DESACTIVADO." << std::endl;
    this->status = false;
  }
  return true;
}

std::string TabletMode::detect_device() {
  const std::string base = "/sys/class/input/";
  for (const auto &entry : fs::directory_iterator(base)) {
    if (!entry.is_directory() ||
        entry.path().filename().string().find("event") == std::string::npos)
      continue;
    std::ifstream ev_file(entry.path() / "device/capabilities/ev");
    std::ifstream sw_file(entry.path() / "device/capabilities/sw");
    if (!ev_file || !sw_file)
      continue;
    unsigned long ev_bits = 0, sw_bits = 0;
    try {
      std::string ev_hex, sw_hex;
      std::getline(ev_file, ev_hex);
      std::getline(sw_file, sw_hex);
      ev_bits = std::stoul(ev_hex, nullptr, 16);
      sw_bits = std::stoul(sw_hex, nullptr, 16);
    } catch (const std::exception &) {
      continue;
    }
    if ((ev_bits & (1UL << 5)) && (sw_bits & (1UL << 1))) {
      auto str = "/dev/input/" + entry.path().filename().string();
      std::cout << "Dispositivo encontrado: " << str << std::endl;
      return str;
    }
  }
  return "";
}

bool TabletMode::is_active() {
  if (!isValid())
    return this->status;
  unsigned char sw_state[(SW_MAX + 7) / 8];
  memset(sw_state, 0, sizeof(sw_state));
  if (ioctl(fd, EVIOCGSW(sizeof(sw_state)), sw_state) < 0)
    return this->status;
  bool current_hw_tablet_mode =
      (sw_state[SW_TABLET_MODE / 8] >> (SW_TABLET_MODE % 8)) & 1;
  if (current_hw_tablet_mode != this->status) {
    this->status = current_hw_tablet_mode;
    setTabletMode(this->status);
    manage_iio_process();
  }
  return this->status;
}

int main() {
  if (!his || !xdg) {
    std::cerr << "Error: Variables de entorno no definidas." << std::endl;
    return 1;
  }
  TabletMode tabletMode;
  int mon_width = 0, mon_height = 0;
  if (!obtener_info_monitor(mon_width, mon_height)) {
    return 1;
  }
  int min_w = mon_width / 2 - 400;
  int max_w = mon_width / 2 + 400;
  int min_y = mon_height * AREA_DE_MUESTRA / 100;
  lanzar_dock_inicial();
  bool dockVisible = true;

  ShakeDetector detector;
  bool cursor_agrandado = false;

  while (true) {
    Posicion pos;
    if (!obtener_posicion_cursor(pos)) {
      usleep(1000 * FRECUENCIA_MS);
      continue;
    }
    bool cursorZona = (pos.y > min_y && pos.x >= min_w && pos.x <= max_w);
    auto dockWorkspace = evaluarDock(mon_height, DOCK_HEIGHT);
    bool shouldShowDock = cursorZona || dockWorkspace == EstadoCliente::VISIBLE;
    if (dockWorkspace == EstadoCliente::FULLSCREEN) {
      shouldShowDock = false;
    }
    if (shouldShowDock && !dockVisible) {
      mostrar_dock();
      dockVisible = true;
    } else if (!shouldShowDock && dockVisible) {
      ocultar_dock();
      dockVisible = false;
    }
    if (tabletMode.is_active()) {
      std::cout << "Modo tablet activado desactivando funcionalidad"
                << std::endl;
      if (cursor_agrandado) {
        disminuir_tamano();
        cursor_agrandado = false;
      }
      usleep(1000 * FRECUENCIA_MS);
      continue;
    }

#ifdef DETECT_SHAKE
    bool sacudida_activa = detector.update({(double)pos.x, (double)pos.y});
    if (sacudida_activa && !cursor_agrandado) {
      std::cout << "Sacudida detectada, aumentando tamaño del cursor."
                << std::endl;
      aumentar_tamano();
      cursor_agrandado = true;
    } else if (!sacudida_activa && cursor_agrandado) {
      std::cout << "Sacudida no detectada, disminuyendo tamaño del cursor."
                << std::endl;
      disminuir_tamano();
      cursor_agrandado = false;
    }
#endif

    usleep(1000 * FRECUENCIA_MS);
  }
  return 0;
}
