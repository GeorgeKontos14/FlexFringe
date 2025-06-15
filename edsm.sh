for i in {1..100}; do
  echo "Running on file $i"
  ./build/flexfringe --ini ini/edsm.ini ./data/stamina_split/${i}_training.txt.dat --outputfile edsm_runs/edsm$i
  ./build/flexfringe --ini ini/edsm.ini --mode predict --predicttype 1 --aptafile ./edsm_runs/edsm$i.final.json ./data/stamina_split/${i}_test.txt.dat
done