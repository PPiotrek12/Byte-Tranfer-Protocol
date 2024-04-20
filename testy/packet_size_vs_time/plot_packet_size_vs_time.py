import pandas as pd
import plotly.express as px


results = pd.read_csv("packet_size_vs_time.csv")
px.line(
    results,
    x="size",
    y="time",
    title="Packet size vs time",
    labels={"size": "Packet size", "time": "Time"},
).show()

