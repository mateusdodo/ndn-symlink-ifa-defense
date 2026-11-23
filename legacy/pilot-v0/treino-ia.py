import pandas as pd
import numpy as np
import os
import glob
import joblib

# Machine Learning
from sklearn.model_selection import train_test_split, cross_val_score
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import classification_report, confusion_matrix

# Visualização de Dados (Matplotlib + Seaborn)
import matplotlib.pyplot as plt
import seaborn as sns
from sklearn.tree import plot_tree

# Configuração visual do Seaborn para gráficos mais elegantes
sns.set_theme(style="whitegrid", palette="muted")

# =====================================================================
# 1. FUNÇÃO OTIMIZADA PARA EXTRAIR FEATURES DO NDNSIM
# =====================================================================
def extrair_features_ndnsim(caminho_arquivo, label):
    data = {}
    try:
        with open(caminho_arquivo, 'r') as f:
            header = f.readline() # Pula o cabeçalho
            for line in f:
                parts = line.strip().split()
                
                if len(parts) < 8: continue
                
                time_val = float(parts[0])
                node = parts[1]
                tipo_pacote = parts[4]
                pacotes_raw = float(parts[7]) 
                
                if node == 'Router':
                    if time_val not in data:
                        data[time_val] = {'InInterests': 0.0, 'OutData': 0.0, 'InNacks': 0.0}
                        
                    if tipo_pacote in data[time_val]:
                        data[time_val][tipo_pacote] += pacotes_raw
                        
        linhas = []
        for t in sorted(data.keys()):
            in_i = data[t]['InInterests']
            out_d = data[t]['OutData']
            in_n = data[t]['InNacks']
            
            pit_leak = max(0, in_i - out_d) 
            
            linhas.append({
                'InInterests': in_i,
                'OutData': out_d,
                'InNacks': in_n,
                'PIT_Leak': pit_leak,
                'Label': int(label)
            })
            
        return pd.DataFrame(linhas)
    except Exception as e:
        print(f"  [X] Erro ao ler o arquivo {caminho_arquivo}: {e}")
        return None

# =====================================================================
# 2. INTERFACE DE TERMINAL (MENU INTERATIVO)
# =====================================================================
def main():
    print("="*60)
    print("🧠 PIPELINE DE TREINAMENTO E VISUALIZAÇÃO IA - NDN IFA")
    print("="*60)
    
    datasets = []
    arquivos = glob.glob("rate-trace-bw_*-freq_*.txt")
    
    if not arquivos:
        print("\n[!] Nenhum arquivo de simulação encontrado na pasta atual.")
        return
        
    print(f"\n--- PASSO 1: Carregando {len(arquivos)} arquivos automaticamente ---")
    
    for caminho in arquivos:
        label = 0 if "freq_0.1" in caminho else 1
        df = extrair_features_ndnsim(caminho, label)
        if df is not None and not df.empty:
            datasets.append(df)
            
    print(f"[OK] Todos os arquivos foram processados!")

    # =====================================================================
    # 3. UNIFICAÇÃO E TREINAMENTO
    # =====================================================================
    print("\n--- PASSO 2: Unificando o Dataset ---")
    dataset_completo = pd.concat(datasets, ignore_index=True)
    print(f"Total de amostras de tempo (0.1s): {len(dataset_completo)}")
    print(f"Amostras Normais (0): {len(dataset_completo[dataset_completo['Label'] == 0])}")
    print(f"Amostras Ataque  (1): {len(dataset_completo[dataset_completo['Label'] == 1])}")

    feature_cols = ['InInterests', 'OutData', 'InNacks', 'PIT_Leak']
    X = dataset_completo[feature_cols] 
    y = dataset_completo['Label']

    print("\n--- PASSO 3: Treinamento do Modelo (Random Forest) ---")
    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42, stratify=y)
    
    clf = RandomForestClassifier(n_estimators=100, max_depth=10, random_state=42)
    clf.fit(X_train, y_train)
    print("[OK] Modelo treinado com sucesso!")

    # =====================================================================
    # 4. AVALIAÇÃO CIENTÍFICA (Métricas)
    # =====================================================================
    print("\n--- PASSO 4: Avaliação do Modelo ---")
    scores = cross_val_score(clf, X, y, cv=10)
    print(f"Acurácia Média (10-Fold CV): {scores.mean() * 100:.2f}% (+/- {scores.std() * 100:.2f}%)")
    
    y_pred = clf.predict(X_test)
    print("\nRelatório de Classificação Detalhado:")
    print(classification_report(y_test, y_pred, target_names=['Normal (0)', 'Ataque (1)']))

    # =====================================================================
    # 5. GERAÇÃO DE GRÁFICOS (MATPLOTLIB + SEABORN)
    # =====================================================================
    print("\n--- PASSO 5: Gerando Gráficos de Alta Resolução ---")
    
    # 5.1 Gráfico de Importância das Features (Seaborn)
    plt.figure(figsize=(8, 5))
    importancias = pd.DataFrame({'Feature': feature_cols, 'Importância': clf.feature_importances_})
    importancias = importancias.sort_values('Importância', ascending=False)
    
    sns.barplot(x='Importância', y='Feature', data=importancias, hue='Feature', legend=False)
    plt.title('Importância das Características (Feature Importance)', fontsize=14, pad=15)
    plt.xlabel('Grau de Importância', fontsize=12)
    plt.ylabel('Características', fontsize=12)
    plt.tight_layout()
    plt.savefig('grafico_importancia_features.png', dpi=300) # Salva em 300 DPI para o artigo
    print("[OK] Gráfico 'grafico_importancia_features.png' salvo!")

    # 5.2 Matriz de Confusão (Seaborn Heatmap)
    plt.figure(figsize=(6, 5))
    cm = confusion_matrix(y_test, y_pred)
    sns.heatmap(cm, annot=True, fmt='d', cmap='Blues', cbar=False, 
                xticklabels=['Normal', 'Ataque'], yticklabels=['Normal', 'Ataque'])
    plt.title('Matriz de Confusão do Random Forest', fontsize=14, pad=15)
    plt.xlabel('Rótulo Previsto pela IA', fontsize=12)
    plt.ylabel('Rótulo Real (Verdadeiro)', fontsize=12)
    plt.tight_layout()
    plt.savefig('grafico_matriz_confusao.png', dpi=300)
    print("[OK] Gráfico 'grafico_matriz_confusao.png' salvo!")

    # 5.3 Visualização da Árvore de Decisão
    # Usamos max_depth=3 para a imagem não ficar gigante e ilegível no PDF
    plt.figure(figsize=(20, 10))
    arvore_exemplo = clf.estimators_[0]
    plot_tree(arvore_exemplo, 
              feature_names=feature_cols, 
              class_names=['Normal', 'Ataque'], 
              filled=True, rounded=True, fontsize=10, max_depth=3)
    plt.title('Estrutura de Decisão Lógica da IA (1 Árvore da Floresta)', fontsize=16)
    plt.savefig('grafico_arvore_decisao.png', dpi=300, bbox_inches='tight')
    print("[OK] Gráfico 'grafico_arvore_decisao.png' salvo!")

    # =====================================================================
    # 6. SALVAR O MODELO
    # =====================================================================
    print("\n--- PASSO 6: Salvar o Modelo ---")
    nome_modelo = input("Nome para salvar o modelo (ex: modelo_completo.pkl) ou Enter para pular: ").strip()
    
    if nome_modelo:
        if not nome_modelo.endswith('.pkl'):
            nome_modelo += '.pkl'
        joblib.dump(clf, nome_modelo)
        print(f"\n SUCESSO! Modelo robusto salvo como '{nome_modelo}'.")
    else:
        print("\nModelo não foi salvo. Fim da execução.")

if __name__ == "__main__":
    main()