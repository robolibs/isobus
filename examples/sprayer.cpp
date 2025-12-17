#include "isobus/hardware_integration/can_hardware_interface.hpp"
#include "isobus/hardware_integration/socket_can_interface.hpp"
#include "isobus/isobus/can_network_manager.hpp"
#include "isobus/isobus/can_partnered_control_function.hpp"
#include "isobus/isobus/isobus_device_descriptor_object_pool.hpp"
#include "isobus/isobus/isobus_standard_data_description_indices.hpp"
#include "isobus/isobus/isobus_task_controller_client.hpp"

#include <atomic>
#include <cassert>
#include <csignal>
#include <iostream>
#include <memory>
#include <sstream>
#include <thread>
#include <vector>

static std::atomic_bool running = true;

void signal_handler(int) { running = false; }

class SectionControlImplementSimulator {
  public:
    static constexpr std::uint16_t MAX_NUMBER_SECTIONS_SUPPORTED = 256;
    static constexpr std::uint8_t NUMBER_SECTIONS_PER_CONDENSED_MESSAGE = 16;

    enum class ImplementDDOPObjectIDs : std::uint16_t {
        Device = 0,
        MainDeviceElement,
        DeviceActualWorkState,
        RequestDefaultProcessData,
        DeviceTotalTime,
        Connector,
        ConnectorXOffset,
        ConnectorYOffset,
        ConnectorType,
        SprayBoom,
        ActualWorkState,
        ActualWorkingWidth,
        AreaTotal,
        SetpointWorkState,
        SectionControlState,
        BoomXOffset,
        BoomYOffset,
        BoomZOffset,
        Section1,
        SectionMax = Section1 + (MAX_NUMBER_SECTIONS_SUPPORTED - 1),
        Section1XOffset,
        SectionXOffsetMax = Section1XOffset + (MAX_NUMBER_SECTIONS_SUPPORTED - 1),
        Section1YOffset,
        SectionYOffsetMax = Section1YOffset + (MAX_NUMBER_SECTIONS_SUPPORTED - 1),
        Section1Width,
        SectionWidthMax = Section1Width + (MAX_NUMBER_SECTIONS_SUPPORTED - 1),
        ActualCondensedWorkingState1To16,
        ActualCondensedWorkingState17To32,
        ActualCondensedWorkingState33To48,
        ActualCondensedWorkingState49To64,
        ActualCondensedWorkingState65To80,
        ActualCondensedWorkingState81To96,
        ActualCondensedWorkingState97To112,
        ActualCondensedWorkingState113To128,
        ActualCondensedWorkingState129To144,
        ActualCondensedWorkingState145To160,
        ActualCondensedWorkingState161To176,
        ActualCondensedWorkingState177To192,
        ActualCondensedWorkingState193To208,
        ActualCondensedWorkingState209To224,
        ActualCondensedWorkingState225To240,
        ActualCondensedWorkingState241To256,
        SetpointCondensedWorkingState1To16,
        SetpointCondensedWorkingState17To32,
        SetpointCondensedWorkingState33To48,
        SetpointCondensedWorkingState49To64,
        SetpointCondensedWorkingState65To80,
        SetpointCondensedWorkingState81To96,
        SetpointCondensedWorkingState97To112,
        SetpointCondensedWorkingState113To128,
        SetpointCondensedWorkingState129To144,
        SetpointCondensedWorkingState145To160,
        SetpointCondensedWorkingState161To176,
        SetpointCondensedWorkingState177To192,
        SetpointCondensedWorkingState193To208,
        SetpointCondensedWorkingState209To224,
        SetpointCondensedWorkingState225To240,
        SetpointCondensedWorkingState241To256,
        LiquidProduct,
        TankCapacity,
        TankVolume,
        LifetimeApplicationVolumeTotal,
        PrescriptionControlState,
        ActualCulturalPractice,
        TargetRate,
        ActualRate,
        AreaPresentation,
        TimePresentation,
        ShortWidthPresentation,
        LongWidthPresentation,
        VolumePresentation,
        VolumePerAreaPresentation
    };

    SectionControlImplementSimulator() = default;

    void set_number_of_sections(std::uint8_t value) { sectionStates.resize(value); }

    std::uint8_t get_number_of_sections() const { return sectionStates.size(); }

    void set_section_state(std::uint8_t index, bool value) {
        if (index < sectionStates.size()) {
            sectionStates.at(index) = value;
        }
    }

    bool get_section_state(std::uint8_t index) const {
        assert(index < sectionStates.size());
        return sectionStates.at(index);
    }

    std::uint32_t get_actual_rate() const {
        bool anySectionOn = false;
        for (std::uint8_t i = 0; i < get_number_of_sections(); i++) {
            if (true == get_section_state(i)) {
                anySectionOn = true;
                break;
            }
        }
        return targetRate * (anySectionOn ? 1 : 0);
    }

    void set_target_rate(std::uint32_t value) { targetRate = value; }

    bool get_actual_work_state() const { return setpointWorkState; }

    void set_target_work_state(bool value) { setpointWorkState = value; }

    constexpr std::uint32_t get_prescription_control_state() const { return 1; }

    constexpr std::uint32_t get_section_control_state() const { return 1; }

    bool create_ddop(std::shared_ptr<isobus::DeviceDescriptorObjectPool> poolToPopulate,
                     isobus::NAME clientName) const {
        bool retVal = true;
        std::uint16_t elementCounter = 0;
        constexpr std::int32_t BOOM_WIDTH = 36576;
        assert(0 != get_number_of_sections());
        const std::int32_t SECTION_WIDTH = (BOOM_WIDTH / get_number_of_sections());
        poolToPopulate->clear();

        constexpr std::array<std::uint8_t, 7> localizationData = {'e', 'n', 0x50, 0x00, 0x55, 0x55, 0xFF};

        retVal &= poolToPopulate->add_device("AgIsoStack++ Test", "1.0.0", "123", "A++1.0", localizationData,
                                             std::vector<std::uint8_t>(), clientName.get_full_name());
        retVal &= poolToPopulate->add_device_element(
            "Sprayer", elementCounter++, 0, isobus::task_controller_object::DeviceElementObject::Type::Device,
            static_cast<std::uint16_t>(ImplementDDOPObjectIDs::MainDeviceElement));
        retVal &= poolToPopulate->add_device_process_data(
            "Actual Work State", static_cast<std::uint16_t>(isobus::DataDescriptionIndex::ActualWorkState),
            isobus::NULL_OBJECT_ID,
            static_cast<std::uint8_t>(
                isobus::task_controller_object::DeviceProcessDataObject::PropertiesBit::MemberOfDefaultSet),
            static_cast<std::uint8_t>(
                isobus::task_controller_object::DeviceProcessDataObject::AvailableTriggerMethods::OnChange),
            static_cast<std::uint16_t>(ImplementDDOPObjectIDs::DeviceActualWorkState));
        retVal &= poolToPopulate->add_device_process_data(
            "Request Default PD", static_cast<std::uint16_t>(ImplementDDOPObjectIDs::RequestDefaultProcessData),
            isobus::NULL_OBJECT_ID, 0,
            static_cast<std::uint8_t>(
                isobus::task_controller_object::DeviceProcessDataObject::AvailableTriggerMethods::Total),
            static_cast<std::uint16_t>(ImplementDDOPObjectIDs::RequestDefaultProcessData));
        retVal &= poolToPopulate->add_device_process_data(
            "Total Time", static_cast<std::uint16_t>(isobus::DataDescriptionIndex::EffectiveTotalTime),
            static_cast<std::uint16_t>(ImplementDDOPObjectIDs::TimePresentation),
            static_cast<std::uint8_t>(
                isobus::task_controller_object::DeviceProcessDataObject::PropertiesBit::MemberOfDefaultSet) |
                static_cast<std::uint8_t>(
                    isobus::task_controller_object::DeviceProcessDataObject::PropertiesBit::Settable),
            static_cast<std::uint8_t>(
                isobus::task_controller_object::DeviceProcessDataObject::AvailableTriggerMethods::Total),
            static_cast<std::uint16_t>(ImplementDDOPObjectIDs::DeviceTotalTime));
        retVal &= poolToPopulate->add_device_element(
            "Connector", elementCounter++, static_cast<std::uint16_t>(ImplementDDOPObjectIDs::MainDeviceElement),
            isobus::task_controller_object::DeviceElementObject::Type::Connector,
            static_cast<std::uint16_t>(ImplementDDOPObjectIDs::Connector));
        retVal &= poolToPopulate->add_device_process_data(
            "Connector X", static_cast<std::uint16_t>(isobus::DataDescriptionIndex::DeviceElementOffsetX),
            static_cast<std::uint16_t>(ImplementDDOPObjectIDs::ShortWidthPresentation),
            static_cast<std::uint8_t>(isobus::task_controller_object::DeviceProcessDataObject::PropertiesBit::Settable),
            0, static_cast<std::uint16_t>(ImplementDDOPObjectIDs::ConnectorXOffset));
        retVal &= poolToPopulate->add_device_process_data(
            "Connector Y", static_cast<std::uint16_t>(isobus::DataDescriptionIndex::DeviceElementOffsetY),
            static_cast<std::uint16_t>(ImplementDDOPObjectIDs::ShortWidthPresentation),
            static_cast<std::uint8_t>(isobus::task_controller_object::DeviceProcessDataObject::PropertiesBit::Settable),
            0, static_cast<std::uint16_t>(ImplementDDOPObjectIDs::ConnectorYOffset));
        retVal &= poolToPopulate->add_device_property(
            "Type", 9, static_cast<std::uint16_t>(isobus::DataDescriptionIndex::ConnectorType), isobus::NULL_OBJECT_ID,
            static_cast<std::uint16_t>(ImplementDDOPObjectIDs::ConnectorType));

        retVal &= poolToPopulate->add_device_element(
            "Boom", elementCounter++, static_cast<std::uint16_t>(ImplementDDOPObjectIDs::MainDeviceElement),
            isobus::task_controller_object::DeviceElementObject::Type::Function,
            static_cast<std::uint16_t>(ImplementDDOPObjectIDs::SprayBoom));
        retVal &= poolToPopulate->add_device_property(
            "Offset X", 0, static_cast<std::uint16_t>(isobus::DataDescriptionIndex::DeviceElementOffsetX),
            static_cast<std::uint16_t>(ImplementDDOPObjectIDs::ShortWidthPresentation),
            static_cast<std::uint16_t>(ImplementDDOPObjectIDs::BoomXOffset));
        retVal &= poolToPopulate->add_device_property(
            "Offset Y", 0, static_cast<std::uint16_t>(isobus::DataDescriptionIndex::DeviceElementOffsetY),
            static_cast<std::uint16_t>(ImplementDDOPObjectIDs::ShortWidthPresentation),
            static_cast<std::uint16_t>(ImplementDDOPObjectIDs::BoomYOffset));
        retVal &= poolToPopulate->add_device_property(
            "Offset Z", 0, static_cast<std::uint16_t>(isobus::DataDescriptionIndex::DeviceElementOffsetZ),
            static_cast<std::uint16_t>(ImplementDDOPObjectIDs::ShortWidthPresentation),
            static_cast<std::uint16_t>(ImplementDDOPObjectIDs::BoomZOffset));
        retVal &= poolToPopulate->add_device_process_data(
            "Actual Working Width", static_cast<std::uint16_t>(isobus::DataDescriptionIndex::ActualWorkingWidth),
            static_cast<std::uint16_t>(ImplementDDOPObjectIDs::LongWidthPresentation),
            static_cast<std::uint8_t>(
                isobus::task_controller_object::DeviceProcessDataObject::PropertiesBit::MemberOfDefaultSet),
            static_cast<std::uint8_t>(
                isobus::task_controller_object::DeviceProcessDataObject::AvailableTriggerMethods::OnChange),
            static_cast<std::uint16_t>(ImplementDDOPObjectIDs::ActualWorkingWidth));
        retVal &= poolToPopulate->add_device_process_data(
            "Setpoint Work State", static_cast<std::uint16_t>(isobus::DataDescriptionIndex::SetpointWorkState),
            isobus::NULL_OBJECT_ID,
            static_cast<std::uint8_t>(
                isobus::task_controller_object::DeviceProcessDataObject::PropertiesBit::MemberOfDefaultSet) |
                static_cast<std::uint8_t>(
                    isobus::task_controller_object::DeviceProcessDataObject::PropertiesBit::Settable),
            static_cast<std::uint8_t>(
                isobus::task_controller_object::DeviceProcessDataObject::AvailableTriggerMethods::OnChange),
            static_cast<std::uint16_t>(ImplementDDOPObjectIDs::SetpointWorkState));
        retVal &= poolToPopulate->add_device_process_data(
            "Area Total", static_cast<std::uint16_t>(isobus::DataDescriptionIndex::TotalArea),
            static_cast<std::uint16_t>(ImplementDDOPObjectIDs::AreaPresentation),
            static_cast<std::uint8_t>(
                isobus::task_controller_object::DeviceProcessDataObject::PropertiesBit::MemberOfDefaultSet),
            static_cast<std::uint8_t>(
                isobus::task_controller_object::DeviceProcessDataObject::AvailableTriggerMethods::Total),
            static_cast<std::uint16_t>(ImplementDDOPObjectIDs::AreaTotal));
        retVal &= poolToPopulate->add_device_process_data(
            "Section Control State", static_cast<std::uint16_t>(isobus::DataDescriptionIndex::SectionControlState),
            isobus::NULL_OBJECT_ID,
            static_cast<std::uint8_t>(
                isobus::task_controller_object::DeviceProcessDataObject::PropertiesBit::MemberOfDefaultSet) |
                static_cast<std::uint8_t>(
                    isobus::task_controller_object::DeviceProcessDataObject::PropertiesBit::Settable),
            static_cast<std::uint8_t>(
                isobus::task_controller_object::DeviceProcessDataObject::AvailableTriggerMethods::OnChange) |
                static_cast<std::uint8_t>(
                    isobus::task_controller_object::DeviceProcessDataObject::AvailableTriggerMethods::TimeInterval),
            static_cast<std::uint16_t>(ImplementDDOPObjectIDs::SectionControlState));

        retVal &= poolToPopulate->add_device_element("Product", elementCounter++, 9,
                                                     isobus::task_controller_object::DeviceElementObject::Type::Bin,
                                                     static_cast<std::uint16_t>(ImplementDDOPObjectIDs::LiquidProduct));
        retVal &= poolToPopulate->add_device_process_data(
            "Tank Capacity", static_cast<std::uint16_t>(isobus::DataDescriptionIndex::MaximumVolumeContent),
            static_cast<std::uint16_t>(ImplementDDOPObjectIDs::VolumePresentation),
            static_cast<std::uint8_t>(
                isobus::task_controller_object::DeviceProcessDataObject::PropertiesBit::MemberOfDefaultSet),
            static_cast<std::uint8_t>(
                isobus::task_controller_object::DeviceProcessDataObject::AvailableTriggerMethods::OnChange) |
                static_cast<std::uint8_t>(
                    isobus::task_controller_object::DeviceProcessDataObject::AvailableTriggerMethods::TimeInterval),
            static_cast<std::uint16_t>(ImplementDDOPObjectIDs::TankCapacity));
        retVal &= poolToPopulate->add_device_process_data(
            "Tank Volume", static_cast<std::uint16_t>(isobus::DataDescriptionIndex::ActualVolumeContent),
            static_cast<std::uint16_t>(ImplementDDOPObjectIDs::VolumePresentation),
            static_cast<std::uint8_t>(
                isobus::task_controller_object::DeviceProcessDataObject::PropertiesBit::MemberOfDefaultSet) |
                static_cast<std::uint8_t>(
                    isobus::task_controller_object::DeviceProcessDataObject::PropertiesBit::Settable),
            static_cast<std::uint8_t>(
                isobus::task_controller_object::DeviceProcessDataObject::AvailableTriggerMethods::OnChange) |
                static_cast<std::uint8_t>(
                    isobus::task_controller_object::DeviceProcessDataObject::AvailableTriggerMethods::TimeInterval),
            static_cast<std::uint16_t>(ImplementDDOPObjectIDs::TankVolume));
        retVal &= poolToPopulate->add_device_process_data(
            "Lifetime Total Volume",
            static_cast<std::uint16_t>(isobus::DataDescriptionIndex::LifetimeApplicationTotalVolume),
            static_cast<std::uint16_t>(ImplementDDOPObjectIDs::VolumePresentation),
            static_cast<std::uint8_t>(
                isobus::task_controller_object::DeviceProcessDataObject::PropertiesBit::MemberOfDefaultSet),
            static_cast<std::uint8_t>(
                isobus::task_controller_object::DeviceProcessDataObject::AvailableTriggerMethods::Total),
            static_cast<std::uint16_t>(ImplementDDOPObjectIDs::LifetimeApplicationVolumeTotal));
        retVal &= poolToPopulate->add_device_process_data(
            "Rx Control State", static_cast<std::uint16_t>(isobus::DataDescriptionIndex::PrescriptionControlState),
            isobus::NULL_OBJECT_ID,
            static_cast<std::uint8_t>(
                isobus::task_controller_object::DeviceProcessDataObject::PropertiesBit::MemberOfDefaultSet) |
                static_cast<std::uint8_t>(
                    isobus::task_controller_object::DeviceProcessDataObject::PropertiesBit::Settable),
            static_cast<std::uint8_t>(
                isobus::task_controller_object::DeviceProcessDataObject::AvailableTriggerMethods::OnChange) |
                static_cast<std::uint8_t>(
                    isobus::task_controller_object::DeviceProcessDataObject::AvailableTriggerMethods::TimeInterval),
            static_cast<std::uint16_t>(ImplementDDOPObjectIDs::PrescriptionControlState));
        retVal &= poolToPopulate->add_device_process_data(
            "Target Rate",
            static_cast<std::uint16_t>(isobus::DataDescriptionIndex::SetpointVolumePerAreaApplicationRate),
            static_cast<std::uint16_t>(ImplementDDOPObjectIDs::VolumePerAreaPresentation),
            static_cast<std::uint8_t>(
                isobus::task_controller_object::DeviceProcessDataObject::PropertiesBit::MemberOfDefaultSet) |
                static_cast<std::uint8_t>(
                    isobus::task_controller_object::DeviceProcessDataObject::PropertiesBit::Settable),
            static_cast<std::uint8_t>(
                isobus::task_controller_object::DeviceProcessDataObject::AvailableTriggerMethods::OnChange),
            static_cast<std::uint16_t>(ImplementDDOPObjectIDs::TargetRate));
        retVal &= poolToPopulate->add_device_process_data(
            "Actual Rate", static_cast<std::uint16_t>(isobus::DataDescriptionIndex::ActualVolumePerAreaApplicationRate),
            static_cast<std::uint16_t>(ImplementDDOPObjectIDs::VolumePerAreaPresentation),
            static_cast<std::uint8_t>(
                isobus::task_controller_object::DeviceProcessDataObject::PropertiesBit::MemberOfDefaultSet),
            static_cast<std::uint8_t>(
                isobus::task_controller_object::DeviceProcessDataObject::AvailableTriggerMethods::OnChange) |
                static_cast<std::uint8_t>(
                    isobus::task_controller_object::DeviceProcessDataObject::AvailableTriggerMethods::TimeInterval),
            static_cast<std::uint16_t>(ImplementDDOPObjectIDs::ActualRate));
        retVal &= poolToPopulate->add_device_property(
            "Operation Type", 3, static_cast<std::uint16_t>(isobus::DataDescriptionIndex::ActualCulturalPractice),
            isobus::NULL_OBJECT_ID, static_cast<std::uint16_t>(ImplementDDOPObjectIDs::ActualCulturalPractice));

        for (std::uint_fast8_t i = 0; i < get_number_of_sections(); i++) {
            std::int32_t individualSectionWidth = BOOM_WIDTH / get_number_of_sections();
            std::ostringstream ss;
            ss << "Section " << static_cast<int>(i);
            retVal &= poolToPopulate->add_device_element(
                ss.str(), elementCounter++, 9, isobus::task_controller_object::DeviceElementObject::Type::Section,
                static_cast<std::uint16_t>(ImplementDDOPObjectIDs::Section1) + i);
            retVal &= poolToPopulate->add_device_property(
                "Offset X", -20, static_cast<std::uint16_t>(isobus::DataDescriptionIndex::DeviceElementOffsetX),
                static_cast<std::uint16_t>(ImplementDDOPObjectIDs::LongWidthPresentation),
                static_cast<std::uint16_t>(ImplementDDOPObjectIDs::Section1XOffset) + i);
            retVal &= poolToPopulate->add_device_property(
                "Offset Y", ((-BOOM_WIDTH) / 2) + (i * SECTION_WIDTH) + (SECTION_WIDTH / 2),
                static_cast<std::uint16_t>(isobus::DataDescriptionIndex::DeviceElementOffsetY),
                static_cast<std::uint16_t>(ImplementDDOPObjectIDs::LongWidthPresentation),
                static_cast<std::uint16_t>(ImplementDDOPObjectIDs::Section1YOffset) + i);
            retVal &= poolToPopulate->add_device_property(
                "Width", individualSectionWidth,
                static_cast<std::uint16_t>(isobus::DataDescriptionIndex::ActualWorkingWidth),
                static_cast<std::uint16_t>(ImplementDDOPObjectIDs::LongWidthPresentation),
                static_cast<std::uint16_t>(ImplementDDOPObjectIDs::Section1Width) + i);
            auto section = std::static_pointer_cast<isobus::task_controller_object::DeviceElementObject>(
                poolToPopulate->get_object_by_id(i + static_cast<std::uint16_t>(ImplementDDOPObjectIDs::Section1)));
            section->add_reference_to_child_object(static_cast<std::uint16_t>(ImplementDDOPObjectIDs::Section1YOffset) +
                                                   i);
            section->add_reference_to_child_object(static_cast<std::uint16_t>(ImplementDDOPObjectIDs::Section1XOffset) +
                                                   i);
            section->add_reference_to_child_object(static_cast<std::uint16_t>(ImplementDDOPObjectIDs::Section1Width) +
                                                   i);
        }

        std::uint16_t sectionCounter = 0;
        while (sectionCounter < get_number_of_sections()) {
            retVal &= poolToPopulate->add_device_process_data(
                "Actual Work State 1-16",
                static_cast<std::uint16_t>(isobus::DataDescriptionIndex::ActualCondensedWorkState1_16) +
                    (sectionCounter / NUMBER_SECTIONS_PER_CONDENSED_MESSAGE),
                isobus::NULL_OBJECT_ID,
                static_cast<std::uint8_t>(
                    isobus::task_controller_object::DeviceProcessDataObject::PropertiesBit::MemberOfDefaultSet),
                static_cast<std::uint8_t>(
                    isobus::task_controller_object::DeviceProcessDataObject::AvailableTriggerMethods::OnChange),
                static_cast<std::uint16_t>(ImplementDDOPObjectIDs::ActualCondensedWorkingState1To16) +
                    (sectionCounter / NUMBER_SECTIONS_PER_CONDENSED_MESSAGE));
            retVal &= poolToPopulate->add_device_process_data(
                "Setpoint Work State 1-16",
                static_cast<std::uint16_t>(isobus::DataDescriptionIndex::SetpointCondensedWorkState1_16) +
                    (sectionCounter / NUMBER_SECTIONS_PER_CONDENSED_MESSAGE),
                isobus::NULL_OBJECT_ID,
                static_cast<std::uint8_t>(
                    isobus::task_controller_object::DeviceProcessDataObject::PropertiesBit::Settable) |
                    static_cast<std::uint8_t>(
                        isobus::task_controller_object::DeviceProcessDataObject::PropertiesBit::MemberOfDefaultSet),
                static_cast<std::uint8_t>(
                    isobus::task_controller_object::DeviceProcessDataObject::AvailableTriggerMethods::OnChange),
                static_cast<std::uint16_t>(ImplementDDOPObjectIDs::SetpointCondensedWorkingState1To16) +
                    (sectionCounter / NUMBER_SECTIONS_PER_CONDENSED_MESSAGE));
            sectionCounter += NUMBER_SECTIONS_PER_CONDENSED_MESSAGE;
        }

        retVal &= poolToPopulate->add_device_value_presentation(
            "mm", 0, 1.0f, 0, static_cast<std::uint16_t>(ImplementDDOPObjectIDs::ShortWidthPresentation));
        retVal &= poolToPopulate->add_device_value_presentation(
            "m", 0, 0.001f, 0, static_cast<std::uint16_t>(ImplementDDOPObjectIDs::LongWidthPresentation));
        retVal &= poolToPopulate->add_device_value_presentation(
            "m^2", 0, 1.0f, 0, static_cast<std::uint16_t>(ImplementDDOPObjectIDs::AreaPresentation));
        retVal &= poolToPopulate->add_device_value_presentation(
            "L", 0, 0.001f, 0, static_cast<std::uint16_t>(ImplementDDOPObjectIDs::VolumePresentation));
        retVal &= poolToPopulate->add_device_value_presentation(
            "minutes", 0, 1.0f, 1, static_cast<std::uint16_t>(ImplementDDOPObjectIDs::TimePresentation));
        retVal &= poolToPopulate->add_device_value_presentation(
            "L/ha", 0, 0.001f, 1, static_cast<std::uint16_t>(ImplementDDOPObjectIDs::VolumePerAreaPresentation));

        if (retVal) {
            auto sprayer = std::static_pointer_cast<isobus::task_controller_object::DeviceElementObject>(
                poolToPopulate->get_object_by_id(
                    static_cast<std::uint16_t>(ImplementDDOPObjectIDs::MainDeviceElement)));
            auto connector = std::static_pointer_cast<isobus::task_controller_object::DeviceElementObject>(
                poolToPopulate->get_object_by_id(static_cast<std::uint16_t>(ImplementDDOPObjectIDs::Connector)));
            auto boom = std::static_pointer_cast<isobus::task_controller_object::DeviceElementObject>(
                poolToPopulate->get_object_by_id(static_cast<std::uint16_t>(ImplementDDOPObjectIDs::SprayBoom)));
            auto product = std::static_pointer_cast<isobus::task_controller_object::DeviceElementObject>(
                poolToPopulate->get_object_by_id(static_cast<std::uint16_t>(ImplementDDOPObjectIDs::LiquidProduct)));

            sprayer->add_reference_to_child_object(
                static_cast<std::uint16_t>(ImplementDDOPObjectIDs::DeviceActualWorkState));
            sprayer->add_reference_to_child_object(static_cast<std::uint16_t>(ImplementDDOPObjectIDs::DeviceTotalTime));

            connector->add_reference_to_child_object(
                static_cast<std::uint16_t>(ImplementDDOPObjectIDs::ConnectorXOffset));
            connector->add_reference_to_child_object(
                static_cast<std::uint16_t>(ImplementDDOPObjectIDs::ConnectorYOffset));
            connector->add_reference_to_child_object(static_cast<std::uint16_t>(ImplementDDOPObjectIDs::ConnectorType));

            boom->add_reference_to_child_object(static_cast<std::uint16_t>(ImplementDDOPObjectIDs::BoomXOffset));
            boom->add_reference_to_child_object(static_cast<std::uint16_t>(ImplementDDOPObjectIDs::BoomYOffset));
            boom->add_reference_to_child_object(static_cast<std::uint16_t>(ImplementDDOPObjectIDs::BoomZOffset));
            boom->add_reference_to_child_object(static_cast<std::uint16_t>(ImplementDDOPObjectIDs::ActualWorkingWidth));
            boom->add_reference_to_child_object(
                static_cast<std::uint16_t>(ImplementDDOPObjectIDs::SectionControlState));

            sectionCounter = 0;
            while (sectionCounter < get_number_of_sections()) {
                boom->add_reference_to_child_object(
                    static_cast<std::uint16_t>(ImplementDDOPObjectIDs::ActualCondensedWorkingState1To16) +
                    (sectionCounter / NUMBER_SECTIONS_PER_CONDENSED_MESSAGE));
                boom->add_reference_to_child_object(
                    static_cast<std::uint16_t>(ImplementDDOPObjectIDs::SetpointCondensedWorkingState1To16) +
                    (sectionCounter / NUMBER_SECTIONS_PER_CONDENSED_MESSAGE));
                sectionCounter += NUMBER_SECTIONS_PER_CONDENSED_MESSAGE;
            }

            product->add_reference_to_child_object(static_cast<std::uint16_t>(ImplementDDOPObjectIDs::TankCapacity));
            product->add_reference_to_child_object(static_cast<std::uint16_t>(ImplementDDOPObjectIDs::TankVolume));
            product->add_reference_to_child_object(
                static_cast<std::uint16_t>(ImplementDDOPObjectIDs::LifetimeApplicationVolumeTotal));
            product->add_reference_to_child_object(
                static_cast<std::uint16_t>(ImplementDDOPObjectIDs::PrescriptionControlState));
            product->add_reference_to_child_object(
                static_cast<std::uint16_t>(ImplementDDOPObjectIDs::ActualCulturalPractice));
            product->add_reference_to_child_object(static_cast<std::uint16_t>(ImplementDDOPObjectIDs::TargetRate));
            product->add_reference_to_child_object(static_cast<std::uint16_t>(ImplementDDOPObjectIDs::ActualRate));
        }
        return retVal;
    }

    static bool request_value_command_callback(std::uint16_t, std::uint16_t DDI, std::int32_t &value,
                                               void *parentPointer) {
        if (nullptr != parentPointer) {
            auto sim = reinterpret_cast<SectionControlImplementSimulator *>(parentPointer);
            switch (DDI) {
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::MaximumVolumeContent):
                value = 4542494;
                break;
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::ActualVolumeContent):
                value = 3785000;
                break;
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::SectionControlState):
                value = sim->get_section_control_state();
                break;
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::PrescriptionControlState):
                value = sim->get_prescription_control_state();
                break;
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::ActualCondensedWorkState1_16):
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::ActualCondensedWorkState17_32):
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::ActualCondensedWorkState33_48):
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::ActualCondensedWorkState49_64):
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::ActualCondensedWorkState65_80):
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::ActualCondensedWorkState81_96):
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::ActualCondensedWorkState97_112):
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::ActualCondensedWorkState113_128):
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::ActualCondensedWorkState129_144):
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::ActualCondensedWorkState145_160):
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::ActualCondensedWorkState161_176):
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::ActualCondensedWorkState177_192):
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::ActualCondensedWorkState193_208):
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::ActualCondensedWorkState209_224):
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::ActualCondensedWorkState225_240):
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::ActualCondensedWorkState241_256): {
                std::uint16_t sectionIndexOffset =
                    NUMBER_SECTIONS_PER_CONDENSED_MESSAGE *
                    (DDI - static_cast<std::uint16_t>(isobus::DataDescriptionIndex::ActualCondensedWorkState1_16));
                value = 0;
                for (std::uint_fast8_t i = 0; i < NUMBER_SECTIONS_PER_CONDENSED_MESSAGE; i++) {
                    if ((i + (sectionIndexOffset)) < sim->get_number_of_sections()) {
                        value |= ((true == sim->get_section_state(i + sectionIndexOffset))
                                      ? static_cast<std::uint32_t>(0x01)
                                      : static_cast<std::uint32_t>(0x00))
                                 << (2 * i);
                    } else {
                        value |= (static_cast<std::uint32_t>(0x03) << (2 * i));
                    }
                }
            } break;
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::ActualVolumePerAreaApplicationRate):
                value = sim->get_actual_rate();
                break;
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::ActualWorkState):
                value = sim->get_actual_work_state();
                break;
            default:
                value = 0;
                break;
            }
        }
        return true;
    }

    static bool value_command_callback(std::uint16_t, std::uint16_t DDI, std::int32_t processVariableValue,
                                       void *parentPointer) {
        if (nullptr != parentPointer) {
            auto sim = reinterpret_cast<SectionControlImplementSimulator *>(parentPointer);
            switch (DDI) {
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::SetpointCondensedWorkState1_16):
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::SetpointCondensedWorkState17_32):
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::SetpointCondensedWorkState33_48):
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::SetpointCondensedWorkState49_64):
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::SetpointCondensedWorkState65_80):
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::SetpointCondensedWorkState81_96):
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::SetpointCondensedWorkState97_112):
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::SetpointCondensedWorkState113_128):
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::SetpointCondensedWorkState129_144):
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::SetpointCondensedWorkState145_160):
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::SetpointCondensedWorkState161_176):
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::SetpointCondensedWorkState177_192):
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::SetpointCondensedWorkState193_208):
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::SetpointCondensedWorkState209_224):
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::SetpointCondensedWorkState225_240):
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::SetpointCondensedWorkState241_256): {
                std::uint16_t sectionIndexOffset =
                    NUMBER_SECTIONS_PER_CONDENSED_MESSAGE *
                    (DDI - static_cast<std::uint16_t>(isobus::DataDescriptionIndex::SetpointCondensedWorkState1_16));
                for (std::uint_fast8_t i = 0; i < NUMBER_SECTIONS_PER_CONDENSED_MESSAGE; i++) {
                    sim->set_section_state(sectionIndexOffset + i, (0x01 == (processVariableValue >> (2 * i) & 0x03)));
                }
            } break;
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::SetpointVolumePerAreaApplicationRate):
                sim->set_target_rate(processVariableValue);
                break;
            case static_cast<std::uint16_t>(isobus::DataDescriptionIndex::SetpointWorkState):
                sim->set_target_work_state(processVariableValue);
                break;
            default:
                break;
            }
        }
        return true;
    }

  private:
    std::vector<bool> sectionStates;
    std::uint32_t targetRate = 0;
    bool setpointWorkState = true;
};

int main() {
    std::signal(SIGINT, signal_handler);

    auto canDriver = std::make_shared<isobus::SocketCANInterface>("can0");
    if (nullptr == canDriver) {
        std::cout << "Unable to find a CAN driver." << std::endl;
        return -1;
    }

    isobus::CANHardwareInterface::set_number_of_can_channels(1);
    isobus::CANHardwareInterface::assign_can_channel_frame_handler(0, canDriver);

    if ((!isobus::CANHardwareInterface::start()) || (!canDriver->get_is_valid())) {
        std::cout << "Failed to start hardware interface." << std::endl;
        return -2;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    isobus::NAME TestDeviceNAME(0);
    TestDeviceNAME.set_arbitrary_address_capable(true);
    TestDeviceNAME.set_industry_group(2);
    TestDeviceNAME.set_device_class(6);
    TestDeviceNAME.set_function_code(static_cast<std::uint8_t>(isobus::NAME::Function::RateControl));
    TestDeviceNAME.set_identity_number(2);
    TestDeviceNAME.set_ecu_instance(0);
    TestDeviceNAME.set_function_instance(0);
    TestDeviceNAME.set_device_class_instance(0);
    TestDeviceNAME.set_manufacturer_code(1407);

    const isobus::NAMEFilter filterTaskController(isobus::NAME::NAMEParameters::FunctionCode,
                                                  static_cast<std::uint8_t>(isobus::NAME::Function::TaskController));
    const isobus::NAMEFilter filterTaskControllerInstance(isobus::NAME::NAMEParameters::FunctionInstance, 0);
    const isobus::NAMEFilter filterTaskControllerIndustryGroup(
        isobus::NAME::NAMEParameters::IndustryGroup,
        static_cast<std::uint8_t>(isobus::NAME::IndustryGroup::AgriculturalAndForestryEquipment));
    const isobus::NAMEFilter filterTaskControllerDeviceClass(
        isobus::NAME::NAMEParameters::DeviceClass, static_cast<std::uint8_t>(isobus::NAME::DeviceClass::NonSpecific));
    const std::vector<isobus::NAMEFilter> tcNameFilters = {filterTaskController, filterTaskControllerInstance,
                                                           filterTaskControllerIndustryGroup,
                                                           filterTaskControllerDeviceClass};

    auto TestInternalECU = isobus::CANNetworkManager::CANNetwork.create_internal_control_function(TestDeviceNAME, 0);
    auto TestPartnerTC = isobus::CANNetworkManager::CANNetwork.create_partnered_control_function(0, tcNameFilters);

    auto TestTCClient = std::make_shared<isobus::TaskControllerClient>(TestPartnerTC, TestInternalECU, nullptr);

    auto myDDOP = std::make_shared<isobus::DeviceDescriptorObjectPool>();
    bool tcClientStarted = false;

    constexpr std::uint16_t NUMBER_OF_SECTIONS_TO_CREATE = 16;
    SectionControlImplementSimulator rateController;
    rateController.set_number_of_sections(NUMBER_OF_SECTIONS_TO_CREATE);

    std::cout << "Sprayer TC Client Example\n";
    std::cout << "Waiting for TC server...\n\n";

    while (running) {
        if (!tcClientStarted) {
            if (rateController.create_ddop(myDDOP, TestInternalECU->get_NAME())) {
                TestTCClient->configure(myDDOP, 1, NUMBER_OF_SECTIONS_TO_CREATE, 1, true, false, true, false, true);
                TestTCClient->add_request_value_callback(
                    SectionControlImplementSimulator::request_value_command_callback, &rateController);
                TestTCClient->add_value_command_callback(SectionControlImplementSimulator::value_command_callback,
                                                         &rateController);
                TestTCClient->initialize(true);
                tcClientStarted = true;
                std::cout << "TC Client initialized successfully\n";
            } else {
                std::cout << "Failed to create DDOP\n";
                break;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    std::cout << "\nShutting down...\n";
    TestTCClient->terminate();
    isobus::CANHardwareInterface::stop();
    return (!tcClientStarted);
}
