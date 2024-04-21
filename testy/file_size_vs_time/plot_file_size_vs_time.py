import pandas as pd
from plotly.subplots import make_subplots
import plotly.graph_objects as go

results = pd.read_csv("file_size_vs_time.csv")
fig = make_subplots(rows=1, cols=2, subplot_titles=("Rozmiar przesyłanego pliku od czasu", "Rozmiar przesyłanego pliku od liczby przesłanych pakietów"))

fig.add_trace(go.Scatter(x=results["size"], y=results["time"], mode='lines+markers', name='Czas'), row=1, col=1)
fig.add_trace(go.Scatter(x=results["size"], y=results["packets"], mode='lines+markers', name='Liczba przesłanych pakietów'), row=1, col=2)

fig.update_layout(xaxis_title='Rozmiar przesyłanego pliku', yaxis_title='Czas / Liczba przesłanych pakietów')

fig.show()






