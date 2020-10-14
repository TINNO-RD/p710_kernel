#ifndef __TGESTURE_GLOBAL_H__
#define __TGESTURE_GLOBAL_H__

#ifndef KEY_TGESTURE
#define KEY_TGESTURE 249
#endif
//extern u8 gTGesture;
//extern int bEnTGesture;
//extern unsigned int is_TDDI_solution;
//extern unsigned int Need_Module_Reset_Keep_High;
//extern struct input_dev *TGesture_dev;
extern bool get_and_init_TGesture_dev(void);
extern void TGesture_dev_report(char value);
#endif
