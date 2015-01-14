#ifndef RenderWindowUI_H
#define RenderWindowUI_H
 
#include "ui_RenderWindowUI.h"
 
#include <QMainWindow>
 
class RenderWindowUI : public QMainWindow, private Ui::RenderWindowUI {
  	Q_OBJECT
	
	public: 
	  RenderWindowUI();
	  void something();
	 
	public slots: 
  		virtual void slotExit(); 
};
 
#endif