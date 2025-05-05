#!/bin/bash
# This script compiles the LaTeX project using pdflatex and bibtex.

pdflatex main.tex
bibtex main
pdflatex main.tex
pdflatex main.tex

rm -f *.aux *.bbl *.blg *.log *.out *.toc *.lof *.lot *.lol *.loa *.loq *.lox *.lol *.synctex.gz *.acn *.acr *.alg *.glg *.glo *.gls *.ist *.fls *.fdb_latexmk *.dvi *.ps *.spl *.backup *.bak *.swp *.log *.out *.aux

rm -f chapters/*.aux

mv main.pdf Distributed-Storage-Service.pdf