# 🎯 GUIA RÁPIDO - Compilar APK no GitHub

## 📝 Resumo do que foi feito

Implementamos:
- ✅ Thermal Protection (proteção térmica)
- ✅ AI Frame Generation (geração de frames por IA)
- ✅ GitHub Actions workflow (compilação automática)

## 🚀 Como compilar (3 passos simples)

### Passo 1: Criar repositório no GitHub

1. Acesse: https://github.com/new
2. Nome do repositório: `eden` (ou qualquer nome)
3. Deixe como **Público** ou **Privado**
4. **NÃO marque** "Initialize with README"
5. Clique em **"Create repository"**

### Passo 2: Executar o script

No terminal, execute:

```bash
cd projetos/eden
./push-to-github.sh
```

O script vai pedir:
- URL do repositório (exemplo: `https://github.com/seu-usuario/eden.git`)
- Suas credenciais do GitHub (se necessário)

### Passo 3: Baixar o APK

1. Acesse: `https://github.com/seu-usuario/eden/actions`
2. Clique na build "Build Android APK"
3. Aguarde ~15-20 minutos (acompanhe o progresso!)
4. Baixe em **"Artifacts" → "eden-release-apk"**
5. Descompacte o ZIP e instale o APK no seu celular

## 📱 Instalação no Celular

```bash
# Via ADB
adb install app-genshinSpoof-release.apk

# Ou transfira manualmente e instale
```

## 🔧 Alternativa: Compilação Manual

Se preferir fazer manualmente:

```bash
cd projetos/eden

# Adicionar arquivos
git add .github/ COMO_COMPILAR_APK.md src/

# Commit
git commit -m "Add thermal protection and AI frame generation"

# Adicionar repositório (primeira vez)
git remote add origin https://github.com/seu-usuario/eden.git

# Enviar
git push -u origin main
```

## ⚡ Compilação Manual (Executar Workflow)

Depois do push, você também pode:

1. Ir em: `https://github.com/seu-usuario/eden/actions`
2. Clicar em "Build Android APK" (lado esquerdo)
3. Clicar em "Run workflow" (botão verde à direita)
4. Selecionar branch "main"
5. Clicar em "Run workflow"

## 🎮 Testando as Novas Features

Após instalar o APK:

1. Abra o Eden
2. Vá em **Settings (Configurações)**
3. Procure por:
   - 🌡️ **Thermal Protection** - Proteção térmica
   - 🎨 **AI Frame Generation** - Geração de frames

## 📊 Arquivos Criados

```
.github/workflows/build-android.yml  ← Workflow do GitHub Actions
COMO_COMPILAR_APK.md                 ← Documentação completa
GUIA_RAPIDO.md                       ← Este arquivo
push-to-github.sh                    ← Script facilitador
```

## ❓ Problemas Comuns

### "Permission denied" ao executar script
```bash
chmod +x push-to-github.sh
./push-to-github.sh
```

### Autenticação do GitHub falha
Use Personal Access Token:
1. GitHub → Settings → Developer settings → Personal access tokens
2. Generate new token (classic)
3. Marque: `repo`, `workflow`
4. Use o token como senha

### Build falha no GitHub Actions
- Verifique os logs na página de Actions
- Procure por erros em vermelho
- Veja o arquivo `COMO_COMPILAR_APK.md` para soluções

## 🆘 Precisa de Ajuda?

Leia a documentação completa em: `COMO_COMPILAR_APK.md`

---

**Dica:** Salve a URL do seu repositório GitHub para acesso rápido!
