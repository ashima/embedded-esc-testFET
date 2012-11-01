set yrange [0:10]
set y2range [0:4]
set autoscale x
set ytic nomirror
set y2tic border

set ylabel "(V)"
set y2label "(A)"
set xlabel "t (us)"

set term x11 enhanced 0
#set term png enhanced size 1600,1200
#set output "single.png"
set multiplot layout 3,2 title 'single FET firing'
set title 'Ap'; plot 'Ap.vcoil' u 2:3 t 'V_{coil}' w lp, 'Ap.vsup' u 2:3 t 'V_{batt}' w lp, 'Ap.current' u 2:3 t 'I_{batt}' w lp axes x1y2
set title 'An'; plot 'An.vcoil' u 2:3 t 'V_{coil}' w lp, 'An.vsup' u 2:3 t 'V_{batt}' w lp, 'An.current' u 2:3 t 'I_{batt}' w lp axes x1y2
set title 'Bp'; plot 'Bp.vcoil' u 2:3 t 'V_{coil}' w lp, 'Bp.vsup' u 2:3 t 'V_{batt}' w lp, 'Bp.current' u 2:3 t 'I_{batt}' w lp axes x1y2
set title 'Bn'; plot 'Bn.vcoil' u 2:3 t 'V_{coil}' w lp, 'Bn.vsup' u 2:3 t 'V_{batt}' w lp, 'Bn.current' u 2:3 t 'I_{batt}' w lp axes x1y2
set title 'Cp'; plot 'Cp.vcoil' u 2:3 t 'V_{coil}' w lp, 'Cp.vsup' u 2:3 t 'V_{batt}' w lp, 'Cp.current' u 2:3 t 'I_{batt}' w lp axes x1y2
set title 'Cn'; plot 'Cn.vcoil' u 2:3 t 'V_{coil}' w lp, 'Cn.vsup' u 2:3 t 'V_{batt}' w lp, 'Cn.current' u 2:3 t 'I_{batt}' w lp axes x1y2
unset multiplot

set term x11 1
#set output "double.png"
set multiplot layout 3,2 title 'double FET firing'
set title 'Ap-Bn'; plot 'Ap-Bn.vcoil' u 2:3 t 'V_{coil}' w lp, 'Ap-Bn.vsup' u 2:3 t 'V_{battery}' w lp, 'Ap-Bn.current' u 2:3 t 'I_{battery}' w lp axes x1y2
set title 'Ap-Cn'; plot 'Ap-Cn.vcoil' u 2:3 t 'V_{coil}' w lp, 'Ap-Cn.vsup' u 2:3 t 'V_{battery}' w lp, 'Ap-Cn.current' u 2:3 t 'I_{battery}' w lp axes x1y2
set title 'Bp-Cn'; plot 'Bp-Cn.vcoil' u 2:3 t 'V_{coil}' w lp, 'Bp-Cn.vsup' u 2:3 t 'V_{battery}' w lp, 'Bp-Cn.current' u 2:3 t 'I_{battery}' w lp axes x1y2
set title 'Bp-An'; plot 'Bp-An.vcoil' u 2:3 t 'V_{coil}' w lp, 'Bp-An.vsup' u 2:3 t 'V_{battery}' w lp, 'Bp-An.current' u 2:3 t 'I_{battery}' w lp axes x1y2
set title 'Cp-An'; plot 'Cp-An.vcoil' u 2:3 t 'V_{coil}' w lp, 'Cp-An.vsup' u 2:3 t 'V_{battery}' w lp, 'Cp-An.current' u 2:3 t 'I_{battery}' w lp axes x1y2
set title 'Cp-Bn'; plot 'Cp-Bn.vcoil' u 2:3 t 'V_{coil}' w lp, 'Cp-Bn.vsup' u 2:3 t 'V_{battery}' w lp, 'Cp-Bn.current' u 2:3 t 'I_{battery}' w lp axes x1y2
unset multiplot
