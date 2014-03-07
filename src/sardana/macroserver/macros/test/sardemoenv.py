#!/usr/bin/env python

#############################################################################
##
## This file is part of Taurus, a Tango User Interface Library
## 
## http://www.tango-controls.org/static/taurus/latest/doc/html/index.html
##
## Copyright 2011 CELLS / ALBA Synchrotron, Bellaterra, Spain
## 
## Taurus is free software: you can redistribute it and/or modify
## it under the terms of the GNU Lesser General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
## 
## Taurus is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU Lesser General Public License for more details.
## 
## You should have received a copy of the GNU Lesser General Public License
## along with Taurus.  If not, see <http://www.gnu.org/licenses/>.
##
#############################################################################

from taurus.core.taurushelper import Device 
# TODO taurus.core.tango.sardana will be moved by SEP10
from taurus.core.tango.sardana.macroserver import registerExtensions
from taurus.core.util.singleton import Singleton
from sardana import sardanacustomsettings

class SarDemoEnv(Singleton):
    '''Class to get _SAR_DEMO environment variable with cross checking with
       the MacroServer (given by 'UNITTEST_DOOR_NAME') 
    '''
    def __init__(self, door_name=None):
        if door_name is None:
            door_name = getattr(sardanacustomsettings,'UNITTEST_DOOR_NAME')
        registerExtensions()
        try:
            self.door = Device(door_name)
            self.ms = self.door.macro_server
        except ValueError:
            raise 'The  door %s does not exist' %(door_name)

        self.controllers = None        
        self.cts = None
        self.motors = None
        self.pseudos = None
        self.zerods = None
        self.oneds = None
        self.twods = None

        try:
            self.env = self.ms.getEnvironment()['_SAR_DEMO']['elements'] + \
                      list(self.ms.getEnvironment()['_SAR_DEMO']['controllers'])
        except KeyError:
            raise 'sar_demo has not been executed for this door %' %(door_name) 

    def getElements(self, elem_type='all'):
        '''Return the name of sardemo element(s) of given elem type  
        '''
        if elem_type.lower() == 'all':
            return self.env
        if elem_type.lower() == 'moveable':
            return self.getElements('motor') + self.getElements('pseudomotor')
        elems = []
        ms_elems = self.ms.getElementNamesOfType(elem_type)
        [elems.append(elem) for elem in ms_elems if elem in self.env is not None]
        return elems

    def getMoveables(self):
        '''Return the name of moveable(s) defined by SarDemo 
        '''
        return self.getMotors() + self.getPseudoMotors()
        
    def getControllers(self):
        '''Return the name of controllers(s) defined by SarDemo 
        '''
        if not self.controllers:
            self.controllers =  self.getElements('controller')
        return self.controllers
            
    def getCTs(self):
        '''Return the name of counter timer exp channel(s) defined by SarDemo 
        '''
        if not self.cts:
            self.cts =  self.getElements('ctexpchannel')
        return self.cts

    def getMotors(self):
        '''Return the name of motor(s) defined by SarDemo 
        '''
        if not self.motors:
            self.motors =  self.getElements('motor')
        return self.motors

    def getPseudoMotors(self):
        '''Return the name of pseudomotor(s) defined by SarDemo 
        '''
        if not self.pseudos:
            self.pseudos =  self.getElements('pseudomotor')
        return self.pseudos


    def getZerods(self):
        '''Return the name of zerod exp channel(s) defined by SarDemo 
        '''
        if not self.zerods:
            self.zerods =  self.getElements('zerodexpchannel')
        return self.zerods


    def getOneds(self):
        '''Return the name of one exp channel(s) defined by SarDemo 
        '''
        if not self.oneds:
            self.oneds =  self.getElements('onedexpchannel')
        return self.oneds

    def getTwods(self):
        '''Return the name of two exp channel(s) defined by SarDemo 
        '''
        if not self.twods:
            self.twods =  self.getElements('twodexpchannel')
        return self.twods
    
    def changeDoor(self, door_name):
        '''Change the door name and reset all lists 
        '''
        self.__init__(door_name)


if __name__ == '__main__':    
    s = SarDemoEnv()
    print s.env
    print s.getControllers()
    print s.getCTs()
    print s.getMotors()
    print s.getPseudoMotors()
    print s.getZerods()
    print s.getOneds()
    print s.getTwods()
    print s.getElements('Moveable')
    print s.getMoveables()
    print s.getElements()
