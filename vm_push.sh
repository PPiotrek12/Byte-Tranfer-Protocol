cd ..
zip -r 1zad.zip 1zad > /dev/null
cd vagrant
vagrant scp ../1zad.zip vm1:~/ > /dev/null
vagrant scp ../1zad.zip vm2:~/ > /dev/null
vagrant ssh vm1 -c "unzip -o 1zad.zip; cd 1zad; make" > /dev/null
vagrant ssh vm2 -c "unzip -o 1zad.zip; cd 1zad; make" > /dev/null


# sudo tc qdisc add dev eth1 root netem delay 1000ms
# sudo tc qdisc change dev eth1 root netem loss 25%
# sudo tc qdisc del dev eth1 root