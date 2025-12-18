#!/bin/bash
# Script para enviar o projeto para o GitHub e iniciar a compilação

set -e

echo "🚀 Preparando para enviar o projeto Eden para o GitHub..."
echo ""

# Cores para output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Verifica se git está configurado
if ! git config user.name > /dev/null 2>&1; then
    echo -e "${YELLOW}⚠️  Git não está configurado. Configurando...${NC}"
    echo -n "Digite seu nome: "
    read GIT_NAME
    echo -n "Digite seu email: "
    read GIT_EMAIL
    git config --global user.name "$GIT_NAME"
    git config --global user.email "$GIT_EMAIL"
fi

# Verifica se é um repositório git
if [ ! -d .git ]; then
    echo -e "${YELLOW}📦 Inicializando repositório Git...${NC}"
    git init
    echo -e "${GREEN}✅ Repositório inicializado${NC}"
fi

# Adiciona todos os arquivos
echo -e "${BLUE}📝 Adicionando arquivos...${NC}"
git add .github/workflows/build-android.yml
git add COMO_COMPILAR_APK.md
git add src/android/app/src/main/jni/thermal_protection.cpp
git add src/android/app/src/main/jni/thermal_protection.h
git add src/android/app/src/main/jni/native_ai_frame_gen.cpp
git add src/android/app/src/main/jni/CMakeLists.txt
git add src/video_core/renderer_opengl/gl_rasterizer.cpp
git add src/video_core/renderer_opengl/gl_rasterizer.h

echo -e "${GREEN}✅ Arquivos adicionados${NC}"

# Commit
echo -e "${BLUE}💾 Criando commit...${NC}"
git commit -m "Add thermal protection and AI frame generation

Features:
- Thermal protection system for device safety
- OpenGL AI frame generation for better performance
- GitHub Actions workflow for automatic APK building
- Comprehensive documentation

This build requires x86-64 environment (GitHub Actions) due to AAPT2 limitations on ARM64."

echo -e "${GREEN}✅ Commit criado${NC}"

# Verifica se tem remote configurado
if ! git remote get-url origin > /dev/null 2>&1; then
    echo ""
    echo -e "${YELLOW}⚠️  Nenhum repositório remoto configurado${NC}"
    echo ""
    echo "Por favor, crie um repositório no GitHub e cole a URL aqui:"
    echo "Exemplo: https://github.com/seu-usuario/eden.git"
    echo -n "URL do repositório: "
    read REPO_URL
    
    git remote add origin "$REPO_URL"
    echo -e "${GREEN}✅ Remote configurado${NC}"
fi

# Push
echo ""
echo -e "${BLUE}🚀 Enviando para o GitHub...${NC}"
BRANCH=$(git branch --show-current)

if [ -z "$BRANCH" ]; then
    BRANCH="main"
    git branch -M main
fi

echo "Branch: $BRANCH"

if git push -u origin "$BRANCH"; then
    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}✅ Código enviado com sucesso!${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    echo -e "${BLUE}📋 Próximos passos:${NC}"
    echo ""
    echo "1. Acesse: $(git remote get-url origin | sed 's/\.git$//')/actions"
    echo "2. Aguarde a compilação (~15-20 minutos)"
    echo "3. Baixe o APK em 'Artifacts'"
    echo ""
    echo -e "${YELLOW}💡 Dica: Você pode acompanhar o progresso em tempo real!${NC}"
    echo ""
else
    echo ""
    echo -e "${YELLOW}⚠️  Erro ao enviar. Possíveis soluções:${NC}"
    echo ""
    echo "1. Verifique se o repositório existe no GitHub"
    echo "2. Verifique suas credenciais"
    echo "3. Tente: git push -f origin $BRANCH (forçar push)"
    echo ""
fi
