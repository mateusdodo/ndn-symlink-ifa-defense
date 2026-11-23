import os
import glob
import pandas as pd
import numpy as np
from sklearn.model_selection import train_test_split, cross_val_score, KFold
from sklearn.ensemble import RandomForestClassifier
from sklearn.tree import DecisionTreeClassifier
from sklearn.neighbors import KNeighborsClassifier
from sklearn.naive_bayes import GaussianNB
from sklearn.metrics import accuracy_score, precision_score, recall_score, f1_score

path_files = glob.glob("rate-trace-*.txt")
data_frames = []

for file in path_files:
    if "freq_0.1.txt" in file or "defesa" in file:
        label = 0
    elif any(f"freq_{f}.0.txt" in file for f in [200, 1000, 3000, 5000]):
        label = 1
    else:
        continue

    try:
        df_temp = pd.read_csv(file, sep=r'\s+', comment='#')
        df_temp.columns = df_temp.columns.str.strip()
        
        if 'Type' in df_temp.columns and 'Packets' in df_temp.columns:
            df_pivot = df_temp.pivot_table(
                index=['Time', 'Node'],
                columns='Type',
                values='Packets',
                aggfunc='sum',
                fill_value=0
            ).reset_index()
            df_pivot['Attack'] = label
            data_frames.append(df_pivot)
        elif 'InInterests' in df_temp.columns:
            df_temp['Attack'] = label
            data_frames.append(df_temp)
    except Exception:
        continue

if not data_frames:
    raise ValueError("Nenhum dado válido pôde ser processado dos arquivos fornecidos.")

df = pd.concat(data_frames, ignore_index=True).fillna(0)

if 'Node' in df.columns:
    df = df[df['Node'].str.contains('Router', case=False, na=False)]

possible_features = ['InInterests', 'OutInterests', 'InData', 'OutData', 'InNacks', 'OutNacks', 'DropInterests']
features_cols = [c for c in possible_features if c in df.columns]

if 'InInterests' in df.columns and 'OutData' in df.columns:
    df['PIT_Leak'] = (df['InInterests'] - df['OutData']) / (df['InInterests'] + 1e-6)
    features_cols.append('PIT_Leak')

X = df[features_cols]
y = df['Attack']

print(f"Total de amostras consolidadas (após pivot): {len(df)}")
print(f"Features identificadas ({len(features_cols)}): {features_cols}\n")

X_train, X_test, y_train, y_test = train_test_split(
    X, y, test_size=0.30, random_state=42, stratify=y
)

models = {
    "Random Forest": RandomForestClassifier(n_estimators=100, random_state=42),
    "Decision Tree": DecisionTreeClassifier(random_state=42),
    "KNN": KNeighborsClassifier(n_neighbors=5),
    "Naive Bayes": GaussianNB()
}

kf = KFold(n_splits=10, shuffle=True, random_state=42)

print("-" * 72)
print(f"{'Modelo':<15} | {'CV Acc (10-fold)':<16} | {'Acurácia':<8} | {'Precisão':<8} | {'Recall':<8} | {'F1-Score':<8}")
print("-" * 72)

for name, model in models.items():
    cv_acc = cross_val_score(model, X_train, y_train, cv=kf, scoring='accuracy').mean()
    
    model.fit(X_train, y_train)
    y_pred = model.predict(X_test)
    
    acc = accuracy_score(y_test, y_pred)
    prec = precision_score(y_test, y_pred, zero_division=0)
    rec = recall_score(y_test, y_pred, zero_division=0)
    f1 = f1_score(y_test, y_pred, zero_division=0)
    
    print(f"{name:<15} | {cv_acc:.4f}           | {acc:.4f}   | {prec:.4f}   | {rec:.4f}   | {f1:.4f}")