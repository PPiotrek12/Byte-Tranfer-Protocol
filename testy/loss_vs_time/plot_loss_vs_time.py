import pandas as pd
from plotly.subplots import make_subplots
import plotly.graph_objects as go

results = pd.read_csv("loss_vs_time.csv")
fig = make_subplots(rows=1, cols=2, subplot_titles=("Czas od procenta zagubionych pakietów", "Liczba przesłanych pakietów od procenta zagubionych pakietów"))

fig.add_trace(go.Scatter(x=results["loss"], y=results["time"], mode='lines', name='Czas'), row=1, col=1)
fig.add_trace(go.Scatter(x=results["loss"], y=results["packets"], mode='lines', name='Liczba przesłanych pakietów'), row=1, col=2)

fig.update_layout(height = 500, width=1500, xaxis_title='Procent zagubionych pakietów (%)', yaxis_title='Czas (sekundy) / Liczba przesłanych pakietów', showlegend=False)

fig.show()






