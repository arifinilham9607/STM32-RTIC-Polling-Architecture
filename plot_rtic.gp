# Konfigurasi Dasar
set datafile separator ","
set terminal pngcairo size 1200,800 font "Arial,11"
set output "Hasil_Visualisasi_Sistem.png"
set multiplot layout 2,2 title "Analisis Performa Arsitektur RTIC-DMA (STM32 Pure Polling)" font ",14" offset 0,-1

# ---------------------------------------------------------
# Grafik A: ADC Raw vs Wave Filter (S1) - Bukti Akselerasi DSP
# ---------------------------------------------------------
set title "A. Reduksi Noise: Raw ADC vs DSP Biquad Filter" font ",12"
set xlabel "Waktu (ms)"
set ylabel "Amplitudo ADC"
set grid
plot "data_simulasi.csv" using 1:2 with lines lc rgb "gray" title "Raw Sensor 1 (Noise)", \
     "data_simulasi.csv" using 1:3 with lines lc rgb "blue" lw 2 title "DSP Filtered Output"

# ---------------------------------------------------------
# Grafik B: Delta Absolut - Parameter Perubahan
# ---------------------------------------------------------
set title "B. Fluktuasi Delta Absolut Antar Sensor" font ",12"
set xlabel "Waktu (ms)"
set ylabel "Nilai Delta"
plot "data_simulasi.csv" using 1:6 with lines lc rgb "dark-violet" lw 2 title "| Filter S1 - Filter S2 |"

# ---------------------------------------------------------
# Grafik C: Critical Level System State
# ---------------------------------------------------------
set title "C. Evaluasi Event-Triggered (System State)" font ",12"
set xlabel "Waktu (ms)"
set ylabel "Level State"
set yrange [-0.5:3.5]
set ytics 1
set grid y
plot "data_simulasi.csv" using 1:7 with steps lc rgb "red" lw 2 title "State (0=Aman, 1=S1, 2=S2, 3=Total Kritis)"

# ---------------------------------------------------------
# Grafik D: Sinyal Aktuator Digital (S1)
# ---------------------------------------------------------
set title "D. Respon Aktuator Digital (S1)" font ",12"
set xlabel "Waktu (ms)"
set ylabel "Logika Digital"
set yrange [-0.5:1.5]
set ytics 1
plot "data_simulasi.csv" using 1:8 with steps lc rgb "web-green" lw 2 title "Status LED/Relay S1"

unset multiplot