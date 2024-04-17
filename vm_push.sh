cd ..
zip -r 1zad.zip 1zad
cd vagrant
vagrant scp ../1zad.zip vm1:~/
vagrant scp ../1zad.zip vm2:~/
vagrant ssh vm1 -c "unzip 1zad.zip"
vagrant ssh vm2 -c "unzip 1zad.zip"