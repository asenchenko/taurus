//+=============================================================================
//
// file :         PseudoMotor.cpp
//
// description :  C++ source for the PseudoMotor and its commands.
//                The class is derived from Device. It represents the
//                CORBA servant object which will be accessed from the
//                network. All commands which can be executed on the
//                PseudoMotor are implemented in this file.
//
// project :      TANGO Device Server
//
// $Author$
//
// $Revision$
//
// $Log$
// Revision 1.7  2007/08/30 12:40:39  tcoutinho
// - changes to support Pseudo counters.
//
// Revision 1.6  2007/08/23 13:59:23  tcoutinho
// - fix to avoid dead lock
//
// Revision 1.5  2007/08/23 10:33:42  tcoutinho
// - basic pseudo counter check
// - some fixes regarding pseudo motors
//
// Revision 1.4  2007/08/20 06:37:32  tcoutinho
// development commit
//
// Revision 1.3  2007/08/17 15:37:43  tcoutinho
// - fix bug: in case pseudo motor controller is in error
//
// Revision 1.2  2007/08/17 13:11:25  tcoutinho
// - pseudo motor restructure
// - pool base dev class restructure
// - initial commit for pseudo counters
//
// Revision 1.1  2007/08/14 07:58:47  tcoutinho
// New initial version of pseudo motor revised
//
//
// copyleft :     European Synchrotron Radiation Facility
//                BP 220, Grenoble 38043
//                FRANCE
//
//-=============================================================================
//
//  		This file is generated by POGO
//	(Program Obviously used to Generate tango Object)
//
//         (c) - Software Engineering Group - ESRF
//=============================================================================



//===================================================================
//
//	The following table gives the correspondance
//	between commands and method's name.
//
//  Command's name|  Method's name
//	----------------------------------------
//  State         |  dev_state()
//  Status        |  dev_status()
//  Abort         |  abort()
//  MoveRelative  |  move_relative()
//
//===================================================================


#include "PyUtils.h"
#include "CPoolDefs.h"
#include "Utils.h"
#include "PoolUtil.h"
#include "PseudoMotor.h"
#include "PseudoMotorClass.h"
#include "PseudoMotorUtil.h"
#include <tango.h>

namespace PseudoMotor_ns
{

//+----------------------------------------------------------------------------
//
// method : 		PseudoMotor::PseudoMotor(string &s)
//
// description : 	constructor for simulated PseudoMotor
//
// in : - cl : Pointer to the DeviceClass object
//      - s : Device name
//
//-----------------------------------------------------------------------------
PseudoMotor::PseudoMotor(Tango::DeviceClass *cl,string &s)
//:Tango::Device_4Impl(cl,s.c_str())
:Pool_ns::PoolIndBaseDev(cl,s.c_str())
{
    init_cmd = false;
    init_device();
}

PseudoMotor::PseudoMotor(Tango::DeviceClass *cl,const char *s)
//:Tango::Device_4Impl(cl,s)
:Pool_ns::PoolIndBaseDev(cl,s)
{
    init_cmd = false;
    init_device();
}

PseudoMotor::PseudoMotor(Tango::DeviceClass *cl,const char *s,const char *d)
//:Tango::Device_4Impl(cl,s,d)
:Pool_ns::PoolIndBaseDev(cl,s,d)
{
    init_cmd = false;
    init_device();
}
//+----------------------------------------------------------------------------
//
// method : 		PseudoMotor::delete_device()
//
// description : 	will be called at device destruction or at init command.
//
//-----------------------------------------------------------------------------
void PseudoMotor::delete_device()
{
    //	Delete device's allocated object

    DEBUG_STREAM << "Entering delete_device for dev " << get_name() << endl;
//
// A trick to inform client(s) listening on events that the pool device is down.
// Without this trick, the clients will have to wait for 3 seconds before being informed
// This is the Tango device time-out.
// To know that we are executing this code due to a pool shutdown and not due to a
// "Init" command, we are using the polling thread ptr which is cleared in the DS
// shutdown sequence before the device destruction
//

    bool sd = false;

    Tango::Util *tg = Tango::Util::instance();
    if (tg->get_polling_thread_object() == NULL)
    {
        sd = true;
        struct timespec req_sleep;
        req_sleep.tv_sec = 0;
        req_sleep.tv_nsec = 500000000;

        while(get_state() == Tango::MOVING)
        {
cout << "Waiting for end of mov of pseudo motor " << device_name << endl;
            nanosleep(&req_sleep,NULL);
        }
    }
    else
    {
        if (get_state() == Tango::MOVING)
        {
            TangoSys_OMemStream o;
            o << "Init command on pseudo motor device is not allowed while a motor is moving" << ends;

            Tango::Except::throw_exception((const char *)"Motor_InitNotAllowed",o.str(),
                    (const char *)"Motor::delete_device");
        }
    }

    delete mov_mg.mg_proxy;

//
// Delete the device from its controller and from the pool
//

    delete_from_pool();
    delete_utils();
    
    PoolIndBaseDev::delete_device();
}

//+----------------------------------------------------------------------------
//
// method : 		PseudoMotor::init_device()
//
// description : 	will be called at device initialization.
//
//-----------------------------------------------------------------------------
void PseudoMotor::init_device()
{
    INFO_STREAM << "PseudoMotor::PseudoMotor() create device " << device_name << endl;

    // Initialise variables to default values
    //--------------------------------------------
    PoolIndBaseDev::init_device();

    grp_mov = false;
    last_set_pos_valid = false;
    pm_mov = false;
    pm_mov_src = false;

//
// Allocate array to store motor pos.
//
    attr_Position_read = &attr_Position_write;

//
// We will push change event on State and position attributes
//
    Tango::Attribute &state_att = dev_attr->get_attr_by_name("state");
    state_att.set_change_event(true,false);

    Tango::Attribute &pos_att = dev_attr->get_attr_by_name("Position");
    pos_att.set_change_event(true);

//
// Build the PoolBaseUtils class depending on the
// controller type
//
    set_utils(new PseudoMotorUtil(pool_dev));

//
// Inform Pool of our birth
//
    Pool_ns::PseudoMotorPool *pseudo_motor_pool_ptr = new Pool_ns::PseudoMotorPool;
    init_pool_element(pseudo_motor_pool_ptr);

    {
        Tango::AutoTangoMonitor atm(pool_dev);
        pool_dev->add_element(pseudo_motor_pool_ptr);
    }

//
// Inform controller of our birth
//
    if (is_fica_built())
    {
        a_new_child(pseudo_motor_pool_ptr->get_ctrl_id());
    }
    else
        set_state(Tango::FAULT);
}

void PseudoMotor::init_pool_element(Pool_ns::PoolElement *pe)
{
    PoolIndBaseDev::init_pool_element(pe);

    Pool_ns::PseudoMotorPool *pmp =
        static_cast<Pool_ns::PseudoMotorPool *>(pe);

    pmp->motor_group_id = motor_group_id;
    
    Tango::AutoTangoMonitor atm(pool_dev);

//
// Try to build motor group information.
// If we get an exception it just means that the motor group devices have
// not been created yet. Don't worry: The pool has a post_init command which
// will handle this case.
//
    try
    {
        Pool_ns::MotorGroupPool &mg_pool = pool_dev->get_motor_group(motor_group_id);
        mov_mg.mg_proxy = new Tango::DeviceProxy(mg_pool.get_full_name().c_str());
        mov_mg.mg_proxy->set_transparency_reconnection(true);
    }
    catch(...)
    {
        mov_mg.mg_proxy = NULL;
    }

    std::vector<Tango::DevLong>::iterator m_id_ite = motor_list.begin();
    for(;m_id_ite != motor_list.end() ; m_id_ite++)
    {
        try
        {
            Pool_ns::PoolElement &mp = pool_dev->get_motor((Pool_ns::ElementId)*m_id_ite);
            pmp->mot_elts.push_back(mp.get_id());
        }
        catch(...)
        {
//
// We should end up here during the startup if the pseudo motor is using
// another pseudo motor that has not been built yet. We initialize it to InvalidId
// and in the post_init we fix this
//
            pmp->mot_elts.push_back(Pool_ns::InvalidId);
        }
    }
    pmp->update_info();
}

void PseudoMotor::set_siblings(map<int32_t, PseudoMotor*> &s)
{
    siblings.resize(s.size(), NULL);
    map<int32_t, PseudoMotor*>::iterator it = s.begin();
    for(; it != s.end(); ++it)
    {
        siblings[it->first] = it->second;
    }
}

void PseudoMotor::fix_pending_elements(Pool_ns::PseudoMotorPool *pmp)
{
    Pool_ns::MotorGroupPool &mg_pool = pool_dev->get_motor_group(motor_group_id);
    mov_mg.mg_proxy = new Tango::DeviceProxy(mg_pool.get_full_name().c_str());
    mov_mg.mg_proxy->set_transparency_reconnection(true);

    for(std::vector<Pool_ns::PoolElement*>::size_type index = 0; 
        index < pmp->mot_elts.size(); ++index)
    {
        if (pmp->mot_elts[index] == Pool_ns::InvalidId)
        {
            Pool_ns::ElementId id = (Pool_ns::ElementId)motor_list[index];
            Pool_ns::PoolElement &mp = pool_dev->get_motor(id);
            pmp->mot_elts[index] = mp.get_id();
        }
    }
}

//+----------------------------------------------------------------------------
//
// method : 		PseudoMotor::get_device_property()
//
// description : 	Read the device properties from database.
//
//-----------------------------------------------------------------------------
void PseudoMotor::get_device_property()
{
    //	Initialize your default values here (if not done with  POGO).
    //------------------------------------------------------------------
    PoolIndBaseDev::get_device_property();

    //	Read device properties from database.(Automatic code generation)
    //------------------------------------------------------------------
    Tango::DbData	dev_prop;
    dev_prop.push_back(Tango::DbDatum("Motor_list"));
    dev_prop.push_back(Tango::DbDatum("Motor_group_id"));

    //	Call database and extract values
    //--------------------------------------------
    if (Tango::Util::instance()->_UseDb==true)
        get_db_device()->get_property(dev_prop);
    Tango::DbDatum	def_prop, cl_prop;
    PseudoMotorClass	*ds_class =
        (static_cast<PseudoMotorClass *>(get_device_class()));
    int	i = -1;

    //	Try to initialize Motor_list from class property
    cl_prop = ds_class->get_class_property(dev_prop[++i].name);
    if (cl_prop.is_empty()==false)	cl_prop  >>  motor_list;
    //	Try to initialize Motor_list from default device value
    def_prop = ds_class->get_default_device_property(dev_prop[i].name);
    if (def_prop.is_empty()==false)	def_prop  >>  motor_list;
    //	And try to extract Motor_list value from database
    if (dev_prop[i].is_empty()==false)	dev_prop[i]  >>  motor_list;

    //	Try to initialize Motor_group_id from class property
    cl_prop = ds_class->get_class_property(dev_prop[++i].name);
    if (cl_prop.is_empty()==false)	cl_prop  >>  motor_group_id;
    //	Try to initialize Motor_group_id from default device value
    def_prop = ds_class->get_default_device_property(dev_prop[i].name);
    if (def_prop.is_empty()==false)	def_prop  >>  motor_group_id;
    //	And try to extract Motor_group_id value from database
    if (dev_prop[i].is_empty()==false)	dev_prop[i]  >>  motor_group_id;

    //	End of Automatic code generation
    //------------------------------------------------------------------

}
//+----------------------------------------------------------------------------
//
// method : 		PseudoMotor::always_executed_hook()
//
// description : 	method always executed before any command is executed
//
//-----------------------------------------------------------------------------
void PseudoMotor::always_executed_hook()
{
    if (should_be_in_fault())
    {
        set_state(Tango::FAULT);
    }
}

//+----------------------------------------------------------------------------
//
// method : 		PseudoMotor::read_attr_hardware
//
// description : 	Hardware acquisition for attributes.
//
//-----------------------------------------------------------------------------
void PseudoMotor::read_attr_hardware(vector<long> &attr_list)
{
    DEBUG_STREAM << "PseudoMotor::read_attr_hardware(vector<long> &attr_list) entering... "<< endl;
    //	Add your own code here
}

//+------------------------------------------------------------------
/**
 *	method:	PseudoMotor::dev_state
 *
 *	description:	method to execute "State"
 *	This command gets the device state (stored in its <i>device_state</i> data member) and returns it to the caller.
 *
 * @return	State Code
 *
 */
//+------------------------------------------------------------------
Tango::DevState PseudoMotor::dev_state()
{
    DEBUG_STREAM << "PseudoMotor::dev_state(): entering... !" << endl;

    // update_state has already been called by always_executed_hook
    update_state(NULL);

    return get_state();
}

//+------------------------------------------------------------------
/**
 *	method:	Motor::dev_status
 *
 *	description:	method to execute "Status"
 *	This command gets the device status (stored in its <i>device_status</i> data member) and returns it to the caller.
 *
 * @return	Status descrition
 *
 */
//+------------------------------------------------------------------
Tango::ConstDevString PseudoMotor::dev_status()
{
    Tango::ConstDevString	argout = DeviceImpl::dev_status();
    DEBUG_STREAM << "PseudoMotor::dev_status(): entering... !" << endl;

    //	Add your own code to control device here

    PoolIndBaseDev::base_dev_status(argout);

    argout = tmp_status.c_str();
    return argout;
}

double PseudoMotor::get_value(bool cache /* = true */)
{
    if (!cache)
    {
        Tango::Attribute &pos_attr = dev_attr->get_attr_by_name("Position");
        read_Position(pos_attr);
    }
    return *attr_Position_read;
}

//+----------------------------------------------------------------------------
//
// method : 		PseudoMotor::read_Position
//
// description : 	Extract real attribute values for Position acquisition result.
//
//-----------------------------------------------------------------------------
void PseudoMotor::read_Position(Tango::Attribute &attr)
{
    DEBUG_STREAM << "PseudoMotor::read_Position(Tango::Attribute &attr) entering... "<< endl;

///
/// Read the positions from the MotorGroup
///
    Pool_ns::PseudoMotCtrlFiCa *pm_fica = get_pm_fica_ptr();

    vector<double> vec_real_pos;
    try
    {
        Tango::DeviceAttribute mg_attr =
            mov_mg.mg_proxy->read_attribute("Position");
        mg_attr >> vec_real_pos;
        vec_real_pos.resize(motor_list.size());
        
        // Tango 6.1 alternative. For keep compatibility with Tango 6.0
        //mg_attr.extract_read(vec_real_pos);
    }
    SAFE_CATCH(pm_fica->get_name(),"read_Position::motor_group_proxy::read_attribute(position)");

///
/// Pass the real motor positions to the Python Pseudo Controller
/// to get the pseudo motor positions.
///
    {
        Pool_ns::AutoPoolLock lo(pm_fica->get_mon());
        try
        {
            *attr_Position_read = get_pm_ctrl()->CalcPseudo(get_axis(), vec_real_pos);
        }
        SAFE_CATCH(pm_fica->get_name(),"read_Position::CalcPseudo");
    }

///
/// Return the pseudo motor positions
///
    attr.set_value(attr_Position_read);

    Tango::DevState mot_sta = get_state();

    if (mot_sta == Tango::MOVING)
        attr.set_quality(Tango::ATTR_CHANGING);
    else if (mot_sta == Tango::ALARM)
        attr.set_quality(Tango::ATTR_ALARM);
}

//+----------------------------------------------------------------------------
//
// method : 		PseudoMotor::write_Position
//
// description : 	Write Position attribute values to hardware.
//
//-----------------------------------------------------------------------------
void PseudoMotor::write_Position(Tango::WAttribute &attr)
{
    DEBUG_STREAM << "PseudoMotor::write_Position(Tango::WAttribute &attr) entering... "<< endl;

//
// Currently 'grp_mov' is set to check if the desired pseudo motor position exceeds any limit.
// The limits are checked before this method is called so if this code is reached in grv_mov
// mode it means the limits have not been exceeded and so it can return
//
    if(grp_mov == true)
        return;

    abort_cmd_executed = false;

///
/// Read the positions from the MotorGroup
///
    vector<double> prev_motor_pos;
    try
    {
        Tango::DeviceAttribute mg_attr = mov_mg.mg_proxy->read_attribute("Position");
        mg_attr >> prev_motor_pos;
        prev_motor_pos.resize(motor_list.size());
        
        // Tango 6.1 alternative. For keep compatibility with Tango 6.0
        //mg_attr.extract_read(prev_motor_pos);
    }
    catch (Tango::DevFailed &e)
    {
        Tango::Except::re_throw_exception(e,
                (const char *)"PseudoMotor_ExceptReadPosition",
                (const char *)"Internal Motor Group throws exception during read_Position",
                (const char *)"PseudoMotor::read_Position");
    }

    last_set_pos_valid = true;

///
/// Calculate all pseudo motor positions and replace the pseudo position
/// of this motor with the one requested.
/// Then Calculate all physical positions based on recently calculated
/// pseudo positions.
///
    attr.get_write_value(attr_Position_write);

    PseudoMotorController *pm_ctrl = get_pm_ctrl();
    Pool_ns::PseudoMotCtrlFiCa *pm_fica = get_pm_fica_ptr();

    vector<double> motor_pos(pm_fica->get_motor_role_nb());

    {
        vector<double> pm_pos(pm_fica->get_pseudo_motor_role_nb());
        Pool_ns::AutoPoolLock lo(pm_fica->get_mon());
        pm_ctrl->CalcAllPseudo(prev_motor_pos,pm_pos);

        for(vector<PseudoMotor*>::size_type cur_role = 1; 
            cur_role <= siblings.size(); cur_role++)
        {
            if((int32_t)cur_role == get_axis())
            {
                pm_pos[cur_role - 1] = attr_Position_write;
                continue;
            }

            PseudoMotor *sibling = siblings[cur_role - 1];

            if(sibling != NULL)
            {
                try
                {
                    pm_pos[cur_role - 1] = sibling->get_last_position_set();
                }
                catch(Tango::DevFailed &e)
                {
                    DEBUG_STREAM << "Last position SET for " << cur_role << " is not valid"<<endl;
                    // Last set on the sibling is not valid. We use the
                    // calculated sibling position
                }
            }
        }

        pm_ctrl->CalcAllPhysical(pm_pos,motor_pos);

        dbg_dvector(prev_motor_pos, "Previous motor positions = ");
        dbg_dvector(pm_pos, "Pseudo motor positions = ");
        dbg_dvector(motor_pos, "New motor positions = ");
    }

    DEBUG_STREAM << "Inform pseudo motor siblings of pseudo motor movement" << endl;
    inform_siblings_pseudo_motor_mov(true);
    DEBUG_STREAM << "Setting the source of the pseudo motor movement" << endl;
    set_pseudo_motor_mov_src(true);

///
/// Send the physical positions to the Motor Group
///
    try
    {
        Tango::DeviceAttribute motor_pos_attr("Position", motor_pos);
        mov_mg.mg_proxy->write_attribute(motor_pos_attr);
    }
    catch (Tango::DevFailed &e)
    {
        Tango::Except::re_throw_exception(e,
            (const char *)"PseudoMotor_ExceptWritePosition",
            (const char *)"Internal Motor Group throws exception during set_Position",
            (const char *)"PseudoMotor::set_Position");
    }

    Tango::Attribute &state_att = dev_attr->get_attr_by_name("state");
    set_state(Tango::MOVING);
    state_att.fire_change_event();
}

void PseudoMotor::inform_siblings_pseudo_motor_mov(bool enable)
{
    for_each(siblings.begin(),siblings.end(),
             bind2nd(mem_fun(&PseudoMotor::set_pseudo_motor_mov),enable));
}


//+------------------------------------------------------------------
/**
 *	method:	PseudoMotor::abort
 *
 *	description:	method to execute "Abort"
 *	Abort movement of all motors that are moving when the command is executed
 *
 *
 */
//+------------------------------------------------------------------
void PseudoMotor::abort()
{
    DEBUG_STREAM << "PseudoMotor::abort(): entering... !" << endl;

    //	Add your own code to control device here
    base_abort(true);
}

//+------------------------------------------------------------------
/**
 *	method:	PseudoMotor::abort
 *
 *	description:	method to execute "Abort"
 *	Abort movement of all motors that are moving when the command is executed
 *
 *
 */
//+------------------------------------------------------------------
void PseudoMotor::base_abort(bool send_evt)
{
    try
    {
        mov_mg.mg_proxy->command_inout("Abort");
    }
    catch (Tango::DevFailed &e)
    {
        Tango::Except::re_throw_exception(e,
            (const char *)"MotorGroup_ExcepAbort",
            (const char *)"Motor Group throws exception during Abort command",
            (const char *)"PseudoMotor::Abort");
    }
    abort_cmd_executed = true;
}

//+------------------------------------------------------------------
/**
 *	method:	PseudoMotor::pool_elem_changed
 *
 *	description: This method is called when the src object has changed
 *               and an event is generated
 *
 * arg(s) : - evt [in]: The event that has occured
 *          - forward_evt [out]: the new internal event data to be sent
 *                               to all listeners
 */
//+------------------------------------------------------------------

void PseudoMotor::pool_elem_changed(Pool_ns::PoolElemEventList &evt_lst,
                                    Pool_ns::PoolElementEvent &forward_evt)
{
//
// State change from a motor
//
    Pool_ns::PoolElementEvent *evt = evt_lst.back();

    forward_evt.priority = evt->priority;

    switch(evt->type)
    {
        case Pool_ns::StateChange:
        {

            Tango::DevState old_state = get_state();

            Tango::DevState new_state = static_cast<Tango::DevState>(evt->curr.state);
            update_state(&new_state);

            new_state = get_state();

            if(old_state != new_state)
            {
                Tango::AutoTangoMonitor synch(this);
                Tango::MultiAttribute *dev_attrs = get_device_attr();
                Tango::Attribute &state_att = dev_attrs->get_attr_by_name("State");
                state_att.fire_change_event();
            }

            forward_evt.type = Pool_ns::StateChange;
            forward_evt.old.state = Pool_ns::PoolTango::toPool(old_state);
            forward_evt.curr.state = Pool_ns::PoolTango::toPool(new_state);
        }
        break;

//
// Position change event from a motor
//
        case Pool_ns::PositionChange:
        {
            assert(false);
        }
        break;

//
// Position array change event from a motor
//
        case Pool_ns::PositionArrayChange:
        {
//
// The position has been changed by an exterior source. This means
// that the last position value SET to this pseudo motor is no longer valid
//
            Tango::DevState pm_state = get_state();

            if(pm_mov == false && last_set_pos_valid == true)
            {
                last_set_pos_valid = false;
            }

            forward_evt.type = Pool_ns::PositionChange;
            forward_evt.old.position = *attr_Position_read;

            int32_t mot_nb = get_pm_fica_ptr()->get_motor_role_nb();

            vector<double> vec_real_pos;
            vec_real_pos.reserve(mot_nb);

            for(int32_t i=0; i < mot_nb; i++)
                vec_real_pos.push_back(evt->curr.position_array[i]);

///
/// Pass the real motor positions to the Python Pseudo Controller
/// to get the pseudo motor positions.
///
            {
                Pool_ns::AutoPoolLock lo(fica_ptr->get_mon());
                *attr_Position_read =
                    get_pm_ctrl()->CalcPseudo(get_axis(), vec_real_pos);
            }

            Tango::MultiAttribute *attr_list = get_device_attr();
            Tango::Attribute &attr = attr_list->get_attr_by_name ("Position");

            // Make sure the event is sent to all clients
            if(true == evt->priority)
                attr.set_change_event(true,false);

            {
                // get the tango synchronization monitor
                Tango::AutoTangoMonitor synch(this);

                // set the attribute value
                attr.set_value (attr_Position_read);

                // The last position value has priority. We take advantage of this
                // to decide if the position quality should be changing or not
                if (pm_state == Tango::MOVING && !evt->priority)
                    attr.set_quality(Tango::ATTR_CHANGING);
                else if (pm_state == Tango::ALARM)
                    attr.set_quality(Tango::ATTR_ALARM);

                // push the event
                attr.fire_change_event();
            }

            if(true == evt->priority)
                attr.set_change_event(true,true);


            forward_evt.curr.position = *attr_Position_read;
            forward_evt.dim = 1;
        }
        break;

        case Pool_ns::MotionEnded:
        {
            if(pm_mov == true)
                inform_siblings_pseudo_motor_mov(false);
            if(pm_mov_src == true)
                set_pseudo_motor_mov_src(false);
        }
        break;
//
// The structure of the motor group has changed.
//
        case Pool_ns::ElementStructureChange:
        {
            // Nothing needs to be done
        }
        break;
        default:
        {
            assert(false);
        }
        break;
    }
}

//+----------------------------------------------------------------------------
//
// method : 		PseudoMotor::get_last_position_set
//
// description : 	Returns the last position that has been set by a write_position
//
// Throws an exception if:
// - Nobody has performed a write_position before or
// - the motor group has been moved from the outside so the current SET value is no longer valid
//
// Returns the last position that has been set by a write_position
//
//-----------------------------------------------------------------------------
Tango::DevDouble PseudoMotor::get_last_position_set()
{
    if(!last_set_pos_valid)
    {
        TangoSys_OMemStream o;
        o << "The last write position value is not valid" << ends;

        Tango::Except::throw_exception(
                (const char *)"PseudoMotor_InvalidSetPositionValue",o.str(),
                (const char *)"PseudoMotor::get_last_position_set");
    }

    Tango::DevDouble last_set_value;
    Tango::WAttribute &pos_attr = dev_attr->get_w_attr_by_name("Position");
    pos_attr.get_write_value(last_set_value);

    return last_set_value;
}

double PseudoMotor::calc_pseudo(Pool_ns::ElementId id, double phy_pos)
{
///
/// Read the positions from the MotorGroup
///
    double pseudo_pos;
    vector<double> vec_real_pos;
    try
    {
        Tango::DeviceAttribute mg_attr =
            mov_mg.mg_proxy->read_attribute("Position");

        mg_attr >> vec_real_pos;
        vec_real_pos.resize(motor_list.size());
        
        // Tango 6.1 alternative. For keep compatibility with Tango 6.0            
        //mg_attr.extract_read(vec_real_pos);
    }
    catch (Tango::DevFailed &e)
    {
            Tango::Except::re_throw_exception(e,
                    (const char *)"PseudoMotor_ExceptReadPosition",
                    (const char *)"Internal Motor Group throws exception "
                                  "during read_Position",
                    (const char *)"PseudoMotor::read_Position");
    }

///
/// Replace the position of the given motor
///
    long r = get_motor_role(id);
    vec_real_pos[r] = phy_pos;

///
/// Pass the real motor positions to the Python Pseudo Controller
/// to get the pseudo motor positions.
///
    {

        Pool_ns::AutoPoolLock lo(fica_ptr->get_mon());
        pseudo_pos = get_pm_ctrl()->CalcPseudo(get_axis(), vec_real_pos);
    }
    return pseudo_pos;
}

double PseudoMotor::calc_pseudo(string &phy_mot_name, double phy_pos)
{
    return calc_pseudo(pool_dev->get_motor(phy_mot_name).get_id(), phy_pos);
}

void PseudoMotor::update_state(Tango::DevState *mg_state)
{
    if (should_be_in_fault())
    {
        set_state(Tango::FAULT);
    }
    else
    {
        if(mg_state == NULL)
            mov_mg.mg_state = mov_mg.mg_proxy->state();
        else
            mov_mg.mg_state = *mg_state;

        set_state(mov_mg.mg_state);
    }
}

void PseudoMotor::sibling_died(int32_t role)
{
//
// siblings vector may not have been initialized due to controller error
//
    if(role <= (int32_t)siblings.size())
        siblings[role - 1] = NULL;
}

int32_t PseudoMotor::get_motor_role(Pool_ns::ElementId id)
{
    std::vector<Tango::DevLong>::iterator it = find(motor_list.begin(), motor_list.end(), id);

    if (it == motor_list.end())
    {
        TangoSys_OMemStream o;
        o << "No motor with id " << id << " found in motor list" << ends;

        Tango::Except::throw_exception(
                (const char *)"PseudoMotor_MotorNotFound",o.str(),
                (const char *)"PseudoMotor::get_motor_role");
    }
    return distance(motor_list.begin(), it);
}

int32_t PseudoMotor::get_motor_role(const std::string &name)
{
    Pool_ns::PoolElement &pe = pool_dev->get_motor(name);
    int32_t idx = -1;
    try
    {
        idx = get_motor_role(pe.get_id());
    }
    catch(Tango::DevFailed &df)
    {
        TangoSys_OMemStream o;
        o << "No motor with id " << id << " found in motor list" << ends;

        Tango::Except::re_throw_exception(df, "PseudoMotor_MotorNotFound", o.str(),
                                          "PseudoMotor::get_motor_role");
    }
    return idx;
}

//+------------------------------------------------------------------
/**
 *	method:	PseudoMotor::move_relative
 *
 *	description:	method to execute "MoveRelative"
 *	move relative command
 *
 * @param	argin	amount to move
 *
 */
//+------------------------------------------------------------------
void PseudoMotor::move_relative(Tango::DevDouble argin)
{
    DEBUG_STREAM << "PseudoMotor::move_relative(): entering... !" << endl;

    //	Add your own code to control device here
    Tango::Except::throw_exception(
            (const char *)"PseudoMotor_FeatureNotImplemented",
            (const char *)"This feature has not been implementd yet",
            (const char *)"PseudoMotor::move_relative");
}

}	//	namespace
