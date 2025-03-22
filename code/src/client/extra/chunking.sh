g++ chunking.cpp -o program
./program split input.txt folder2 
./program merge output.txt folder2

echo "Comparing input.txt and output.txt"
diff input.txt output.txt

rm program