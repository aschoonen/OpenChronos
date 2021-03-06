#!/usr/bin/python
# encoding: utf-8

from . import fmForm
from . import fmActionForm
import curses


class Popup(fmForm.Form):
    def __init__(self,
        lines = 12, 
        columns=60,
        minimum_lines=None,
        minimum_columns=None,
        *args, **keywords):
        super(Popup, self).__init__(lines = lines, columns=columns, 
        *args, **keywords)
        self.show_atx = 10
        self.show_aty = 2
        
class ActionPopup(fmActionForm.ActionForm, Popup):
    def __init__(self, *args, **keywords):
        Popup.__init__(self, *args, **keywords)
        
class MessagePopup(Popup):
    def __init__(self, *args, **keywords):
        import multiline 
        super(MessagePopup, self).__init__(*args, **keywords)
        self.TextWidget = self.add(multiline.Pager, scroll_exit=True, max_height=self.widget_useable_space()[0]-2)
        

