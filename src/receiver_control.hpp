#ifndef LBJ_RECEIVER_CONTROL_HPP
#define LBJ_RECEIVER_CONTROL_HPP

float actualFreq(float bias);
void handleSync();
void handleCarrier();
void handlePreamble();
void revertFrequency();

#endif
