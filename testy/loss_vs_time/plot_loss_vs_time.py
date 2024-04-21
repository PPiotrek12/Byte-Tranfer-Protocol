import pandas as pd
from plotly.subplots import make_subplots
import plotly.graph_objects as go

results = pd.read_csv("loss_vs_time.csv")
fig = make_subplots(rows=1, cols=2, subplot_titles=("Procent zagubionych pakietów od czasu", "Procent zagubionych pakietów od liczby przesłanych pakietów"))

fig.add_trace(go.Scatter(x=results["loss"], y=results["time"], mode='lines+markers', name='Czas'), row=1, col=1)
fig.add_trace(go.Scatter(x=results["loss"], y=results["packets"], mode='lines+markers', name='Liczba przesłanych pakietów'), row=1, col=2)

fig.update_layout(xaxis_title='Procent zagubionych pakietów', yaxis_title='Czas / Liczba przesłanych pakietów')

fig.show()






