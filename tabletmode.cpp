#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// Using hyprctl keyword plugin:hyprbars:bar_height dont work correctly
bool toggle_bar_height(const std::string &config_path) {
  const std::string SECTION = "hyprbars";
  const std::string KEY = "bar_height";
  const std::string VALUE_VISIBLE = "40";
  const std::string VALUE_HIDDEN = "0";

  std::ifstream infile(config_path);
  if (!infile.is_open()) {
    std::cerr << "❌ No se pudo abrir el archivo: " << config_path << std::endl;
    return false;
  }

  std::vector<std::string> lines;
  std::string line;
  bool in_hyprbars = false;
  bool modified = false;

  while (std::getline(infile, line)) {
    std::string trimmed = line;
    trimmed.erase(0, trimmed.find_first_not_of(" \t"));

    if (trimmed.find(SECTION + " {") != std::string::npos) {
      in_hyprbars = true;
    } else if (in_hyprbars && trimmed.find('}') != std::string::npos) {
      in_hyprbars = false;
    }

    if (in_hyprbars && trimmed.rfind(KEY, 0) == 0) {
      std::string current_value = trimmed.substr(trimmed.find('=') + 1);
      current_value.erase(0, current_value.find_first_not_of(" \t"));
      current_value.erase(current_value.find_last_not_of(" \t") + 1);

      std::string new_value =
          (current_value == VALUE_VISIBLE) ? VALUE_HIDDEN : VALUE_VISIBLE;
      std::string new_line = line.substr(0, line.find('=')) + "= " + new_value;
      lines.push_back(new_line);
      modified = true;
    } else {
      lines.push_back(line);
    }
  }
  infile.close();

  if (!modified) {
    std::cerr << "⚠️ No se encontró '" << KEY << "' dentro de '" << SECTION
              << "'." << std::endl;
    return false;
  }

  // Backup
  try {
    std::filesystem::copy(config_path, config_path + ".bak",
                          std::filesystem::copy_options::overwrite_existing);
  } catch (...) {
    std::cerr << "⚠️ No se pudo crear el archivo de respaldo." << std::endl;
  }

  // Write updated config
  std::ofstream outfile(config_path);
  if (!outfile.is_open()) {
    std::cerr << "❌ No se pudo escribir en el archivo." << std::endl;
    return false;
  }

  for (const auto &l : lines)
    outfile << l << "\n";

  std::cout << "✅ Configuración actualizada. Ejecutando 'hyprctl reload'..."
            << std::endl;

  return true;
}

std::string exec(const char *cmd) {
  std::array<char, 128> buffer;
  std::string result;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
  if (!pipe) {
    throw std::runtime_error("popen() failed!");
  }
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) !=
         nullptr) {
    result += buffer.data();
  }
  return result;
}

void setTabletMode(bool status) {
  std::string command = "ags run-js 'tabletMode();'";
  exec(command.c_str());

  if (status) {
    std::cout << "Tablet mode activated." << std::endl;
  } else {
    std::cout << "Tablet mode deactivated." << std::endl;
  }
  std::string config_path =
      std::string(std::getenv("HOME")) + "/.config/hypr/windowrules.conf";
  toggle_bar_height(config_path);
}
