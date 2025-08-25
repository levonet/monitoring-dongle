#include <ctype.h>
#include <string.h>

#include "info_helper.h"

char *rtrim(char *s) {
    char* back = s + strlen(s);
    while(isspace(*--back));
    *(back + 1) = '\0';
    return s;
}

void string_descriptor(const usb_str_desc_t *str_desc, char *str) {
    int cursor = 0;

    if (str_desc == NULL) {
        str[cursor] = '\0';
        return;
    }

    for (int i = 0; i < str_desc->bLength / 2; i++) {
        /*
        USB String descriptors of UTF-16.
        Right now We just skip any character larger than 0xFF to stay in BMP Basic Latin and Latin-1 Supplement range.
        */
        if (str_desc->wData[i] > 0xFF) {
            continue;
        }
        str[cursor++] = (char)str_desc->wData[i];
    }
    str[cursor] = '\0';
}

// void _usb_cdc_print_desc(const usb_standard_desc_t *_desc) {
//     if (_desc->bDescriptorType != ((USB_CLASS_COMM << 4) | USB_B_DESCRIPTOR_TYPE_INTERFACE )) {
//         // Quietly return in case that this descriptor is not CDC interface descriptor
//         return;
//     }

//     switch (((cdc_header_desc_t *)_desc)->bDescriptorSubtype) {
//     case USB_CDC_DESC_SUBTYPE_HEADER: {
//         cdc_header_desc_t *desc = (cdc_header_desc_t *)_desc;
//         syslog.information.printf("  *** CDC Header Descriptor ***\n");
//         syslog.information.printf("  bcdCDC: %d.%d0\n", ((desc->bcdCDC >> 8) & 0xF), ((desc->bcdCDC >> 4) & 0xF));
//         break;
//     }
//     case USB_CDC_DESC_SUBTYPE_CALL: {
//         cdc_acm_call_desc_t *desc = (cdc_acm_call_desc_t *)_desc;
//         syslog.information.printf("  *** CDC Call Descriptor ***\n");
//         syslog.information.printf("  bmCapabilities: 0x%02X\n", desc->bmCapabilities.val);
//         syslog.information.printf("  bDataInterface: %d\n", desc->bDataInterface);
//         break;
//     }
//     case USB_CDC_DESC_SUBTYPE_ACM: {
//         cdc_acm_acm_desc_t *desc = (cdc_acm_acm_desc_t *)_desc;
//         syslog.information.printf("  *** CDC ACM Descriptor ***\n");
//         syslog.information.printf("  bmCapabilities: 0x%02X\n", desc->bmCapabilities.val);
//         break;
//     }
//     case USB_CDC_DESC_SUBTYPE_UNION: {
//         cdc_union_desc_t *desc = (cdc_union_desc_t *)_desc;
//         syslog.information.printf("  *** CDC Union Descriptor ***\n");
//         syslog.information.printf("  bControlInterface: %d\n", desc->bControlInterface);
//         syslog.information.printf("  bSubordinateInterface[0]: %d\n", desc->bSubordinateInterface[0]);
//         break;
//     }
//     default:
//         syslog.error.printf("Unsupported CDC specific descriptor\n");
//         break;
//     }
// }


    // cdc_acm_host_desc_print(cdc_dev);
    // cdc_dev_t *_cdc_dev = (cdc_dev_t *)cdc_dev;

    // usb_device_handle_t dev_hdl = cdc_dev->dev_hdl;

        // const usb_device_desc_t *device_desc;
        // const usb_config_desc_t *config_desc;
        // ESP_ERROR_CHECK_WITHOUT_ABORT(usb_host_get_device_descriptor(dev_hdl, &device_desc));
        // ESP_ERROR_CHECK_WITHOUT_ABORT(usb_host_get_active_config_descriptor(dev_hdl, &config_desc));

        // syslog.information.printf("*** Device descriptor ***\n");
        // syslog.information.printf("bLength %d\n", device_desc->bLength);
        // syslog.information.printf("bDescriptorType %d\n", device_desc->bDescriptorType);
        // syslog.information.printf("bcdUSB %d.%d0\n", ((device_desc->bcdUSB >> 8) & 0xF), ((device_desc->bcdUSB >> 4) & 0xF));
        // syslog.information.printf("bDeviceClass 0x%x\n", device_desc->bDeviceClass);
        // syslog.information.printf("bDeviceSubClass 0x%x\n", device_desc->bDeviceSubClass);
        // syslog.information.printf("bDeviceProtocol 0x%x\n", device_desc->bDeviceProtocol);
        // syslog.information.printf("bMaxPacketSize0 %d\n", device_desc->bMaxPacketSize0);
        // syslog.information.printf("idVendor 0x%x\n", device_desc->idVendor);
        // syslog.information.printf("idProduct 0x%x\n", device_desc->idProduct);
        // syslog.information.printf("bcdDevice %d.%d0\n", ((device_desc->bcdDevice >> 8) & 0xF), ((device_desc->bcdDevice >> 4) & 0xF));
        // syslog.information.printf("iManufacturer %d\n", device_desc->iManufacturer);
        // syslog.information.printf("iProduct %d\n", device_desc->iProduct);
        // syslog.information.printf("iSerialNumber %d\n", device_desc->iSerialNumber);
        // syslog.information.printf("bNumConfigurations %d\n", device_desc->bNumConfigurations);

/*
        // usb_print_config_descriptor(config_desc, cdc_print_desc);
        //cfg_desc, print_class_descriptor_cb class_specific_cb
        int offset = 0;
        uint16_t wTotalLength = config_desc->wTotalLength;
        const usb_standard_desc_t *next_desc = (const usb_standard_desc_t *)config_desc;

        do {
            switch (next_desc->bDescriptorType) {
            case USB_B_DESCRIPTOR_TYPE_CONFIGURATION:
                // usbh_print_cfg_desc((const usb_config_desc_t *)next_desc);
                printf("*** Configuration descriptor ***\n");
                printf("bLength %d\n", cfg_desc->bLength);
                printf("bDescriptorType %d\n", cfg_desc->bDescriptorType);
                printf("wTotalLength %d\n", cfg_desc->wTotalLength);
                printf("bNumInterfaces %d\n", cfg_desc->bNumInterfaces);
                printf("bConfigurationValue %d\n", cfg_desc->bConfigurationValue);
                printf("iConfiguration %d\n", cfg_desc->iConfiguration);
                printf("bmAttributes 0x%x\n", cfg_desc->bmAttributes);
                printf("bMaxPower %dmA\n", cfg_desc->bMaxPower * 2);
                break;
            case USB_B_DESCRIPTOR_TYPE_INTERFACE:
                // usbh_print_intf_desc((const usb_intf_desc_t *)next_desc);
                printf("\t*** Interface descriptor ***\n");
                printf("\tbLength %d\n", intf_desc->bLength);
                printf("\tbDescriptorType %d\n", intf_desc->bDescriptorType);
                printf("\tbInterfaceNumber %d\n", intf_desc->bInterfaceNumber);
                printf("\tbAlternateSetting %d\n", intf_desc->bAlternateSetting);
                printf("\tbNumEndpoints %d\n", intf_desc->bNumEndpoints);
                printf("\tbInterfaceClass 0x%x\n", intf_desc->bInterfaceClass);
                printf("\tbInterfaceSubClass 0x%x\n", intf_desc->bInterfaceSubClass);
                printf("\tbInterfaceProtocol 0x%x\n", intf_desc->bInterfaceProtocol);
                printf("\tiInterface %d\n", intf_desc->iInterface);
                break;
            case USB_B_DESCRIPTOR_TYPE_ENDPOINT:
                // print_ep_desc((const usb_ep_desc_t *)next_desc);
                const char *ep_type_str;
                int type = ep_desc->bmAttributes & USB_BM_ATTRIBUTES_XFERTYPE_MASK;

                switch (type) {
                case USB_BM_ATTRIBUTES_XFER_CONTROL:
                    ep_type_str = "CTRL";
                    break;
                case USB_BM_ATTRIBUTES_XFER_ISOC:
                    ep_type_str = "ISOC";
                    break;
                case USB_BM_ATTRIBUTES_XFER_BULK:
                    ep_type_str = "BULK";
                    break;
                case USB_BM_ATTRIBUTES_XFER_INT:
                    ep_type_str = "INT";
                    break;
                default:
                    ep_type_str = NULL;
                    break;
                }

                printf("\t\t*** Endpoint descriptor ***\n");
                printf("\t\tbLength %d\n", ep_desc->bLength);
                printf("\t\tbDescriptorType %d\n", ep_desc->bDescriptorType);
                printf("\t\tbEndpointAddress 0x%x\tEP %d %s\n", ep_desc->bEndpointAddress,
                       USB_EP_DESC_GET_EP_NUM(ep_desc),
                       USB_EP_DESC_GET_EP_DIR(ep_desc) ? "IN" : "OUT");
                printf("\t\tbmAttributes 0x%x\t%s\n", ep_desc->bmAttributes, ep_type_str);
                printf("\t\twMaxPacketSize %d\n", USB_EP_DESC_GET_MPS(ep_desc));
                printf("\t\tbInterval %d\n", ep_desc->bInterval);
                break;
            case USB_B_DESCRIPTOR_TYPE_INTERFACE_ASSOCIATION:
                // print_iad_desc((const usb_iad_desc_t *)next_desc);
                printf("*** Interface Association Descriptor ***\n");
                printf("bLength %d\n", iad_desc->bLength);
                printf("bDescriptorType %d\n", iad_desc->bDescriptorType);
                printf("bFirstInterface %d\n", iad_desc->bFirstInterface);
                printf("bInterfaceCount %d\n", iad_desc->bInterfaceCount);
                printf("bFunctionClass 0x%x\n", iad_desc->bFunctionClass);
                printf("bFunctionSubClass 0x%x\n", iad_desc->bFunctionSubClass);
                printf("bFunctionProtocol 0x%x\n", iad_desc->bFunctionProtocol);
                printf("iFunction %d\n", iad_desc->iFunction);
                break;
            default:
                _usb_cdc_print_desc(next_desc);
                break;
            }

            next_desc = usb_parse_next_descriptor(next_desc, wTotalLength, &offset);

        } while (next_desc != NULL);
*/