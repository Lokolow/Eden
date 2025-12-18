# 🚀 Como Compilar o APK do Eden com GitHub Actions

## ✨ O que foi implementado

Este projeto agora inclui:
- ✅ **Thermal Protection** - Proteção térmica inteligente
- ✅ **OpenGL Frame Generation** - Geração de frames por IA para melhor desempenho

## 📋 Pré-requisitos

1. Ter uma conta no GitHub
2. Fazer fork ou push deste repositório para o GitHub

## 🔧 Passos para Compilar

### Opção 1: Compilação Automática (Push)

1. **Faça commit das alterações:**
   ```bash
   git add .
   git commit -m "Add thermal protection and AI frame generation"
   ```

2. **Envie para o GitHub:**
   ```bash
   git push origin main
   ```

3. **Aguarde a compilação:**
   - Acesse: `https://github.com/SEU_USUARIO/SEU_REPO/actions`
   - Veja a build rodando em tempo real
   - Aguarde ~15-20 minutos

4. **Baixe o APK:**
   - Clique na build concluída
   - Na seção "Artifacts", baixe `eden-release-apk`

### Opção 2: Compilação Manual

1. **Acesse Actions no GitHub:**
   - Vá para: `https://github.com/SEU_USUARIO/SEU_REPO/actions`

2. **Execute manualmente:**
   - Clique em "Build Android APK" (lado esquerdo)
   - Clique em "Run workflow" (botão direito)
   - Selecione a branch e clique em "Run workflow"

3. **Baixe o APK:**
   - Aguarde a build completar
   - Baixe o artifact `eden-release-apk`

## 📦 Localização do APK

Após o download do artifact, você terá:
```
eden-release-apk.zip
└── genshinSpoof/
    └── release/
        └── app-genshinSpoof-release.apk  ← Este é o APK!
```

## 🐛 Solução de Problemas

### Build falha com erro de NDK
- Verifique se o NDK 28.2.13676358 está especificado no workflow
- Confira se os CMakeLists.txt estão corretos

### Build falha com erro de dependências
- Verifique sua conexão com internet
- Tente limpar o cache: Settings > Actions > Caches

### APK não aparece nos Artifacts
- Verifique se a build completou com sucesso (✅ verde)
- Confira os logs da etapa "Build Release APK"

## 📱 Instalação no Dispositivo

1. **Habilite "Fontes Desconhecidas":**
   - Configurações > Segurança > Instalar apps desconhecidos

2. **Transfira o APK:**
   ```bash
   adb install app-genshinSpoof-release.apk
   ```
   
   Ou envie via cabo USB/Bluetooth e instale manualmente

3. **Execute e teste:**
   - Abra o app Eden
   - Vá em Configurações
   - Verifique as novas opções de thermal protection e frame gen

## 🎯 Variantes Disponíveis

O projeto compila 3 variantes:
- **genshinSpoofRelease** - Versão com spoofing (recomendado)
- **mainlineRelease** - Versão padrão
- **legacyRelease** - Versão para dispositivos antigos

## 💡 Dicas

- Use a opção "workflow_dispatch" para builds sob demanda
- Os artifacts ficam disponíveis por 30 dias
- Builds levam ~15-20 minutos em média
- Você pode ter múltiplas builds rodando simultaneamente

## 🔗 Links Úteis

- [GitHub Actions Documentation](https://docs.github.com/en/actions)
- [Android NDK Documentation](https://developer.android.com/ndk)
- [Gradle Build Documentation](https://docs.gradle.org/)

---

**Nota:** Este workflow foi criado automaticamente para compilar o projeto em ambiente x86-64, contornando limitações de build em ARM64.
