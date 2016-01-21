limeparms;
active_rf = zeros(1,4);
active_rf = [1 1 1 1];
RF_ACTIVE = ((active_rf(1)*1) + (active_rf(2)*2) + (active_rf(3)*4) + (active_rf(4)*8)) * (2^5);
autocal = [1 1 1 1];
resampling_factor = [2 2 2 2];

%rf_mode = (RXEN+TXEN+TXLPFNORM+TXLPFEN+TXLPF25+RXLPFNORM+RXLPFEN+RXLPF25+LNA1ON+LNAMax+RFBBNORM)*[1 1 1 1];
rf_mode = (RXEN+TXLPFNORM+TXLPFEN+TXLPF25+RXLPFNORM+RXLPFEN+RXLPF25+LNA1ON+LNAMax+RFBBNORM)*active_rf;
rf_mode = rf_mode+((DMAMODE_RX)*active_rf);
freq_rx = 2600000000*active_rf;
%freq_rx = 1912600000*[1 1 1 1];
%freq_rx = 859500000*[1 1 1 1];
freq_tx = freq_rx;
tx_gain = 0*active_rf; %[1 1 1 1];
rx_gain = 0*active_rf; %1 1 1 1];
rf_local= rf_local*active_rf; %[1 1 1 1];
rf_rxdc = rf_rxdc*active_rf; %[1 1 1 1];
%rf_vcocal=rf_vcocal_859*[1 1 1 1];
rf_vcocal=rf_vcocal_19G*active_rf; %1 1 1 1];
eNBflag = 0;
tdd_config = DUPLEXMODE_FDD + TXRXSWITCH_TESTRX + RF_ACTIVE; 
%tdd_config = DUPLEXMODE_FDD + TXRXSWITCH_LSB;
syncmode0 = SYNCMODE_MASTER;
syncmode1 = SYNCMODE_SLAVE;
rffe_rxg_low = 31*active_rf; %[1 1 1 1];
rffe_rxg_final = 63*active_rf; %[1 1 1 1];
rffe_band = B19G_TDD*active_rf; %[1 1 1 1];

n_cards = oarf_get_num_detected_cards;

if (configured==0)
printf('Configuring Card 0\n');
oarf_config_exmimo(0,freq_rx,freq_tx,tdd_config,syncmode0,rx_gain,tx_gain,eNBflag,rf_mode,rf_rxdc,rf_local,rf_vcocal,rffe_rxg_low,rffe_rxg_final,rffe_band,autocal,resampling_factor)
sleep(1);
for card=1:n_cards-1
   printf('Configuring Card %d\n',card);
   oarf_config_exmimo(card,freq_rx,freq_tx,tdd_config,syncmode1,rx_gain,tx_gain,eNBflag,rf_mode,rf_rxdc,rf_local,rf_vcocal,rffe_rxg_low,rffe_rxg_final,rffe_band,autocal,resampling_factor)
   sleep(1);
end
configured=1;
end

s=oarf_get_frame(0);
%s1=oarf_get_frame(1);

%s = [s s1];

if (resampling_factor(1)== 2)
length1 = 76800;
f0 = (7.68*(0:length(s(:,1))/4-1)/(length(s(:,1))/4))-3.84;
axis([-3.84,3.84,40,200]);
elseif (resampling_factor(1) == 1)
length1 = 153600;
f0 = (15.36*(0:length(s(:,1))/2-1)/(length(s(:,1))/2))-7.68;
axis([-7.68,7.68,40,200]);
else
length1 = 307200;
f0 = (30.72*(0:length(s(:,1))-1)/(length(s(:,1))))-15.36;
axis([-15.36,15.36,40,200]);
endif


if (resampling_factor(2)== 2)
length2 = 76800;
f1 = (7.68*(0:length(s(:,2))/4-1)/(length(s(:,2))/4))-3.84;
axis([-3.84,3.84,40,200]);
elseif (resampling_factor(2) == 1)
length2 = 153600;
f1 = (15.36*(0:length(s(:,2))/2-1)/(length(s(:,2))/2))-7.68;
axis([-7.68,7.68,40,200]);
else
length2 = 307200;
f1 = (30.72*(0:length(s(:,2))-1)/(length(s(:,2))))-15.36;
axis([-15.36,15.36,40,200]);
endif

if (resampling_factor(3)== 2)
length3 = 76800;
f2 = (7.68*(0:length(s(:,3))/4-1)/(length(s(:,3))/4))-3.84;
axis([-3.84,3.84,40,200]);
elseif (resampling_factor(3) == 1)
length3 = 153600;
f2 = (15.36*(0:length(s(:,3))/2-1)/(length(s(:,3))/2))-7.68;
axis([-7.68,7.68,40,200]);
else
length3 = 307200;
f2 = (30.72*(0:length(s(:,3))-1)/(length(s(:,3))))-15.36;
axis([-15.36,15.36,40,200]);
endif

if (resampling_factor(4)== 2)
length4 = 76800;
f3 = (7.68*(0:length(s(:,4))/4-1)/(length(s(:,4))/4))-3.84;
axis([-3.84,3.84,40,200]);
elseif (resampling_factor(4) == 1)
length4 = 153600;
f3 = (15.36*(0:length(s(:,4))/2-1)/(length(s(:,4))/2))-7.68;
axis([-7.68,7.68,40,200]);
else
length4 = 307200;
f3 = (30.72*(0:length(s(:,4))-1)/(length(s(:,4))))-15.36;
axis([-15.36,15.36,40,200]);
endif


figure(1)
clf
for card=0:n_cards-1
for chain=0:3
subplot(n_cards,4,card*4+chain+1)
hold off;
plot(f0',20*log10(abs(fftshift(fft(s(1:length1,card*4+chain+1))))));
axis([min(f0) max(f0) 0 160])
end
end

figure(2)
clf
for card=0:n_cards-1
for chain=0:3
subplot(n_cards,4,card*4+chain+1)
hold off;
plot(real(s(1:length1,card*4+chain+1)));
axis([0 length1 -2048 2018]);
end
end
