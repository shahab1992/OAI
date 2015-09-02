close all
colors={'r','g','b','m','c','k'};
f=2;

txsigF0UL
txsig0UL

figure(1)
  plot(abs(txsF0),colors{mod(f,6)+1})
hold on
drawnow
figure(2)
plot(abs(txs0),colors{mod(f,6)+1})
hold on
drawnow
  figure(3)
  plot(20*log10(abs(fftshift(fft(txs0(1:7680) )))),colors{mod(f,6)+1})
hold on
drawnow
