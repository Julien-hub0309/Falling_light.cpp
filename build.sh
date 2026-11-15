#!/bin/bash
# Linux / macOS :  ./build.sh   puis   ./build/falling_light
cd "$(dirname "$0")"

# 1. Compilation de l'application
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
if [ $? -ne 0 ]; then
    echo "Erreur : La compilation a échoué."
    exit 1
fi

echo "OK -> ./build/falling_light"

# 2. Création du raccourci sur le bureau (uniquement sous Linux)
if [ "$(uname)" = "Linux" ]; then
    # Récupération du dossier Bureau (gère aussi les systèmes en français "Bureau" via xdg-user-dir)
    DESKTOP_DIR=$(xdg-user-dir DESKTOP 2>/dev/null)
    if [ -z "$DESKTOP_DIR" ]; then
        DESKTOP_DIR="$HOME/Desktop"
    fi

    # Chemins absolus requis pour que le raccourci fonctionne de n'importe où
    PROJECT_DIR="$(pwd)"
    EXEC_PATH="$PROJECT_DIR/build/falling_light"
    ICON_PATH="$PROJECT_DIR/asset/icon.png"
    DESKTOP_FILE="$DESKTOP_DIR/falling_light.desktop"

    echo "Création du raccourci sur le bureau..."

    # Génération du fichier .desktop
    cat << EOF > "$DESKTOP_FILE"
[Desktop Entry]
Type=Application
Name=Falling Light
Exec=$EXEC_PATH
Icon=$ICON_PATH
Terminal=false
Categories=Game;
EOF

    # Rendre le fichier exécutable (obligatoire sous Linux)
    chmod +x "$DESKTOP_FILE"
    
    # Marquer le raccourci comme de confiance (nécessaire sur certains environnements comme GNOME/Ubuntu)
    gio set "$DESKTOP_FILE" metadata::trusted true 2>/dev/null || true

    echo "Raccourci créé avec succès : $DESKTOP_FILE"
fi