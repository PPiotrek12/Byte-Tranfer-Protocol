import pandas as pd
from plotly.subplots import make_subplots
import plotly.graph_objects as go

tcp_results = pd.read_csv("TCP;100MB/packet_size_vs_time.csv")
udp_results = pd.read_csv("UDPR;100MB/packet_size_vs_time.csv")

fig = make_subplots(rows=1, cols=2, subplot_titles=("Czas od rozmiar pakietu", "Liczba przesłanych pakietów od rozmiaru pakietu"))

fig.add_trace(go.Scatter(name = "Czas TCP", x=tcp_results["size"], y=tcp_results["time"], mode='lines'), row=1, col=1)
fig.add_trace(go.Scatter(name = "Czas UDPr", x=udp_results["size"], y=udp_results["time"], mode='lines'), row=1, col=1)


fig.add_trace(go.Scatter(name = "Pakiety TCP", x=tcp_results["size"], y=tcp_results["packets"], mode='lines'), row=1, col=2)
fig.add_trace(go.Scatter(name = "Pakiety UDPr", x=udp_results["size"], y=udp_results["packets"], mode='lines'), row=1, col=2)

fig.update_layout(height =600, width = 1500, xaxis_title='Rozmiar pakietu (bajty)', yaxis_title='Czas (sekundy) / Liczba przesłanych pakietów')

fig.show()






