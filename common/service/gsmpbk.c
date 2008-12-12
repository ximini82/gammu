/* (c) 2001-2005 by Marcin Wiacek, Michal Cihar... */

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>

#include <gammu-misc.h>
#include <gammu-bitmap.h>

#include "../misc/coding/coding.h"
#include "../misc/misc.h"
#include "gsmpbk.h"
#include "gsmmisc.h"

GSM_MemoryType GSM_StringToMemoryType(const char *s) {
    if (strcmp(s, "ME") == 0)      return MEM_ME;
    else if (strcmp(s, "SM") == 0) return MEM_SM;
    else if (strcmp(s, "ON") == 0) return MEM_ON;
    else if (strcmp(s, "DC") == 0) return MEM_DC;
    else if (strcmp(s, "RC") == 0) return MEM_RC;
    else if (strcmp(s, "MC") == 0) return MEM_MC;
    else if (strcmp(s, "MT") == 0) return MEM_MT;
    else if (strcmp(s, "FD") == 0) return MEM_FD;
    else if (strcmp(s, "VM") == 0) return MEM_VM;
    else if (strcmp(s, "SL") == 0) return MEM_SL;
    else {
		return 0;
    }
}

unsigned char *GSM_PhonebookGetEntryName (const GSM_MemoryEntry *entry)
{
	/* We possibly store here "LastName, FirstName" so allocate enough memory */
	static char     dest[(GSM_PHONEBOOK_TEXT_LENGTH*2+2+1)*2];
	static char     split[] = { '\0', ',', '\0', ' ', '\0', '\0'};
	int	     i;
	int	     first = -1, last = -1, name = -1;
	int	     len = 0;

	for (i = 0; i < entry->EntriesNum; i++) {
		switch (entry->Entries[i].EntryType) {
			case PBK_Text_LastName:
				last = i;
				break;
			case PBK_Text_FirstName:
				first = i;
				break;
			case PBK_Text_Name:
				name = i;
				break;
			default:
				break;
		}
	}

	if (name != -1) {
		CopyUnicodeString(dest, entry->Entries[name].Text);
	} else {
		if (last != -1 && first != -1) {
			len = UnicodeLength(entry->Entries[last].Text);
			CopyUnicodeString(dest, entry->Entries[last].Text);
			CopyUnicodeString(dest + 2*len, split);
			CopyUnicodeString(dest + 2*len + 4, entry->Entries[first].Text);
		} else if (last != -1) {
			CopyUnicodeString(dest, entry->Entries[last].Text);
		} else if (first != -1) {
			CopyUnicodeString(dest, entry->Entries[first].Text);
		} else {
			return NULL;
		}
	}

	return dest;
}

void GSM_PhonebookFindDefaultNameNumberGroup(const GSM_MemoryEntry *entry, int *Name, int *Number, int *Group)
{
	int i;

	*Name   = -1;
	*Number = -1;
	*Group  = -1;
	for (i = 0; i < entry->EntriesNum; i++) {
		switch (entry->Entries[i].EntryType) {
		case PBK_Number_General : if (*Number   == -1) *Number  = i; break;
		case PBK_Text_Name      : if (*Name     == -1) *Name    = i; break;
		case PBK_Caller_Group   : if (*Group    == -1) *Group   = i; break;
		default		 :				    break;
		}
	}
	if ((*Number) == -1) {
		for (i = 0; i < entry->EntriesNum; i++) {
			switch (entry->Entries[i].EntryType) {
				case PBK_Number_Mobile:
				case PBK_Number_Work:
				case PBK_Number_Fax:
				case PBK_Number_Home:
				case PBK_Number_Pager:
				case PBK_Number_Other:
					*Number = i;
					break;
				default:
					break;
			}
			if (*Number != -1) break;
		}
	}
	if ((*Name) == -1) {
		for (i = 0; i < entry->EntriesNum; i++) {
			if (entry->Entries[i].EntryType != PBK_Text_LastName) continue;
			*Name = i;
			break;
		}
	}
	if ((*Name) == -1) {
		for (i = 0; i < entry->EntriesNum; i++) {
			if (entry->Entries[i].EntryType != PBK_Text_FirstName) continue;
			*Name = i;
			break;
		}
	}
}


GSM_Error GSM_EncodeVCARD(char *Buffer, const size_t buff_len, size_t *Length, GSM_MemoryEntry *pbk, const bool header, const GSM_VCardVersion Version)
{
	int Name, Number, Group, i;
	int firstname = -1, lastname = -1;
	int address = -1, city = -1, state = -1, zip = -1, country = -1;
	int workaddress = -1, workcity = -1, workstate = -1, workzip = -1, workcountry = -1;
	unsigned char buffer[1024];
	int pos;
	bool ignore;
	GSM_Error error;
	GSM_BinaryPicture *bitmap;

	GSM_PhonebookFindDefaultNameNumberGroup(pbk, &Name, &Number, &Group);

	if (header) {
		error = VC_StoreLine(Buffer, buff_len, Length, "BEGIN:VCARD");
		if (error != ERR_NONE) return error;
	}
	if (Version == Nokia_VCard10 || Version == SonyEricsson_VCard10) {
		if (Name != -1) {
			error = VC_StoreLine(Buffer, buff_len, Length, "N:%s",
				DecodeUnicodeString(pbk->Entries[Name].Text));
			if (error != ERR_NONE) return error;
		}
		if (Number != -1) {
			error = VC_StoreLine(Buffer, buff_len, Length, "TEL:%s",
				DecodeUnicodeString(pbk->Entries[Number].Text));
			if (error != ERR_NONE) return error;
		}
	} else if (Version == Nokia_VCard21 || Version == SonyEricsson_VCard21) {
		if (header) {
			error = VC_StoreLine(Buffer, buff_len, Length, "VERSION:2.1");
			if (error != ERR_NONE) return error;
		}

		error = VC_StoreLine(Buffer, buff_len, Length, "X-GAMMU-LOCATION:%d", pbk->Location);
		if (error != ERR_NONE) return error;

		error = VC_StoreLine(Buffer, buff_len, Length, "X-GAMMU-MEMORY:%s", GSM_MemoryTypeToString(pbk->MemoryType));
		if (error != ERR_NONE) return error;

		for (i=0; i < pbk->EntriesNum; i++) {
			ignore = false;
			pbk->Entries[i].AddError = ERR_NONE;
			switch(pbk->Entries[i].EntryType) {
				case PBK_Text_Name:
					error = VC_Store(Buffer, buff_len, Length, "N");
					if (error != ERR_NONE) return error;
					break;
				case PBK_Text_NickName:
					error = VC_Store(Buffer, buff_len, Length, "NICKNAME");
					if (error != ERR_NONE) return error;
					break;
				case PBK_Text_FormalName:
					error = VC_Store(Buffer, buff_len, Length, "FN");
					if (error != ERR_NONE) return error;
					break;
				case PBK_Text_FirstName:
					firstname = i;
					ignore = true;
					break;
				case PBK_Text_LastName:
					lastname = i;
					ignore = true;
					break;
				case PBK_Text_StreetAddress:
					address = i;
					ignore = true;
					break;
				case PBK_Text_City:
					city = i;
					ignore = true;
					break;
				case PBK_Text_State:
					state = i;
					ignore = true;
					break;
				case PBK_Text_Zip:
					zip = i;
					ignore = true;
					break;
				case PBK_Text_Country:
					country = i;
					ignore = true;
					break;
				case PBK_Text_WorkStreetAddress:
					workaddress = i;
					ignore = true;
					break;
				case PBK_Text_WorkCity:
					workcity = i;
					ignore = true;
					break;
				case PBK_Text_WorkState:
					workstate = i;
					ignore = true;
					break;
				case PBK_Text_WorkZip:
					workzip = i;
					ignore = true;
					break;
				case PBK_Text_WorkCountry:
					workcountry = i;
					ignore = true;
					break;
				case PBK_Date:
					error = VC_StoreDate(Buffer, buff_len, Length, &(pbk->Entries[i].Date), "BDAY");
					if (error != ERR_NONE) return error;
					ignore = true;
					break;
				case PBK_LastModified:
					error = VC_StoreDateTime(Buffer, buff_len, Length, &(pbk->Entries[i].Date), "LAST-MODIFIED");
					if (error != ERR_NONE) return error;
					ignore = true;
					break;
				case PBK_Number_General:
				case PBK_Number_Other:
				case PBK_Number_Pager:
				case PBK_Number_Mobile  :
				case PBK_Number_Work    :
				case PBK_Number_Fax     :
				case PBK_Number_Home    :
				case PBK_Number_Messaging    :
					if (UnicodeLength(pbk->Entries[i].Text) == 0) {
						ignore = true;
						break;
					}
					error = VC_Store(Buffer, buff_len, Length, "TEL");
					if (error != ERR_NONE) return error;

					if (Version != SonyEricsson_VCard21 && Number == i) {
						error = VC_Store(Buffer, buff_len, Length, ";PREF");
						if (error != ERR_NONE) return error;
					}
					switch (pbk->Entries[i].EntryType) {
						case PBK_Number_Other:
							error = VC_Store(Buffer, buff_len, Length, ";OTHER");
							if (error != ERR_NONE) return error;
							break;
						case PBK_Number_Pager:
							error = VC_Store(Buffer, buff_len, Length, ";PAGER");
							if (error != ERR_NONE) return error;
							break;
						case PBK_Number_Mobile:
							error = VC_Store(Buffer, buff_len, Length, ";CELL");
							if (error != ERR_NONE) return error;
							break;
						case PBK_Number_Work:
							error = VC_Store(Buffer, buff_len, Length, ";WORK");
							if (error != ERR_NONE) return error;
							break;
						case PBK_Number_Fax:
							error = VC_Store(Buffer, buff_len, Length, ";FAX");
							if (error != ERR_NONE) return error;
							break;
						case PBK_Number_Home:
							error = VC_Store(Buffer, buff_len, Length, ";HOME");
							if (error != ERR_NONE) return error;
							break;
						case PBK_Number_Messaging:
							error = VC_Store(Buffer, buff_len, Length, ";MSG");
							if (error != ERR_NONE) return error;
							break;
						default:
							break;
					}
					break;
				case PBK_Text_Note      :
					if (UnicodeLength(pbk->Entries[i].Text) == 0) {
						ignore = true;
						break;
					}
					error = VC_Store(Buffer, buff_len, Length, "NOTE");
					if (error != ERR_NONE) return error;
					break;
				case PBK_Text_Postal    :
					if (UnicodeLength(pbk->Entries[i].Text) == 0) {
						ignore = true;
						break;
					}
					/* Don't ask why. Nokia phones save postal address
					 * double - once like LABEL, second like ADR
					 */
					error = VC_StoreText(Buffer, buff_len, Length, pbk->Entries[i].Text, "LABEL", false);
					if (error != ERR_NONE) return error;
					error = VC_Store(Buffer, buff_len, Length, "ADR;HOME");
					if (error != ERR_NONE) return error;
					break;
				case PBK_Text_WorkPostal    :
					if (UnicodeLength(pbk->Entries[i].Text) == 0) {
						ignore = true;
						break;
					}
					error = VC_Store(Buffer, buff_len, Length, "ADR;WORK");
					if (error != ERR_NONE) return error;
					break;
				case PBK_Text_Email     :
				case PBK_Text_Email2    :
					if (UnicodeLength(pbk->Entries[i].Text) == 0) {
						ignore = true;
						break;
					}
					error = VC_Store(Buffer, buff_len, Length, "EMAIL");
					if (error != ERR_NONE) return error;
					break;
				case PBK_Text_URL       :
					if (UnicodeLength(pbk->Entries[i].Text) == 0) {
						ignore = true;
						break;
					}
					error = VC_Store(Buffer, buff_len, Length, "URL");
					if (error != ERR_NONE) return error;
					break;
				case PBK_Text_LUID      :
					if (UnicodeLength(pbk->Entries[i].Text) == 0) {
						ignore = true;
						break;
					}
					error = VC_Store(Buffer, buff_len, Length, "X-IRMC-LUID");
					if (error != ERR_NONE) return error;
					break;
				case PBK_Text_JobTitle:
					if (UnicodeLength(pbk->Entries[i].Text) == 0) {
						ignore = true;
						break;
					}
					error = VC_Store(Buffer, buff_len, Length, "TITLE");
					if (error != ERR_NONE) return error;
					break;
				case PBK_Text_Company:
					if (UnicodeLength(pbk->Entries[i].Text) == 0) {
						ignore = true;
						break;
					}
					error = VC_Store(Buffer, buff_len, Length, "ORG");
					if (error != ERR_NONE) return error;
					break;
				case PBK_Photo:
					bitmap = &(pbk->Entries[i].Picture);
					error = VC_Store(Buffer, buff_len, Length, "PHOTO;TYPE=");
					if (error != ERR_NONE) return error;
					switch (bitmap->Type) {
						case PICTURE_BMP:
							error = VC_Store(Buffer, buff_len, Length, "BMP;");
							break;
						case PICTURE_GIF:
							error = VC_Store(Buffer, buff_len, Length, "GIF;");
							break;
						case PICTURE_JPG:
							error = VC_Store(Buffer, buff_len, Length, "JPEG;");
							break;
						case PICTURE_ICN:
							error = VC_Store(Buffer, buff_len, Length, "ICO;");
							break;
						case PICTURE_PNG:
							error = VC_Store(Buffer, buff_len, Length, "PNG;");
							break;
						default:
							dbgprintf("Unknown picture format: %d\n", bitmap->Type);
							error = VC_Store(Buffer, buff_len, Length, "UNKNOWN;");
							break;
					}
					if (error != ERR_NONE) return error;
					error = VC_Store(Buffer, buff_len, Length, "ENCODING=BASE64:");
					if (error != ERR_NONE) return error;
					error = VC_StoreBase64(Buffer, buff_len, Length, bitmap->Buffer, bitmap->Length);
					if (error != ERR_NONE) return error;
					ignore = true;
					break;
				case PBK_Caller_Group:
					error = VC_StoreLine(Buffer, buff_len, Length, "X-CALLER-GROUP:%d", pbk->Entries[i].Number);
					if (error != ERR_NONE) return error;
					break;
				case PBK_Private:
					error = VC_StoreLine(Buffer, buff_len, Length, "X-PRIVATE:%d", pbk->Entries[i].Number);
					if (error != ERR_NONE) return error;
					break;
				/* Not supported fields */
				case PBK_Category:
				case PBK_RingtoneID:
				case PBK_PictureID:
				case PBK_Text_UserID:
				case PBK_CallLength:
				case PBK_Text_Custom1:
				case PBK_Text_Custom2:
				case PBK_Text_Custom3:
				case PBK_Text_Custom4:
				case PBK_Text_PictureName:
				case PBK_PushToTalkID:
					pbk->Entries[i].AddError = ERR_NOTSUPPORTED;
					ignore = true;
					break;
			}
			if (!ignore) {
				error = VC_StoreText(Buffer, buff_len, Length, pbk->Entries[i].Text, "", false);
				if (error != ERR_NONE) return error;
			}
		}
		/* Save name if it is composed from parts */
		if (firstname != -1 || lastname != -1) {
			pos = 0;
			if (lastname != -1) {
				CopyUnicodeString(buffer + 2*pos, pbk->Entries[lastname].Text);
				pos += UnicodeLength(pbk->Entries[lastname].Text);
			}
			buffer[2*pos] = 0;
			buffer[2*pos + 1] = ';';
			pos++;
			if (firstname != -1) {
				CopyUnicodeString(buffer + 2*pos, pbk->Entries[firstname].Text);
				pos += UnicodeLength(pbk->Entries[firstname].Text);
			}
			buffer[2*pos] = 0;
			buffer[2*pos + 1] = 0;
			error = VC_StoreText(Buffer, buff_len, Length, buffer, "N", false);
			if (error != ERR_NONE) return error;
		}
		/* Save workaddress if it is composed from parts */
		if (workaddress != -1 || workcity != -1 || workstate != -1 || workzip != -1 || workcountry != -1) {
			pos = 0;
			buffer[2*pos] = 0;
			buffer[2*pos + 1] = ';';
			pos++;
			buffer[2*pos] = 0;
			buffer[2*pos + 1] = ';';
			pos++;
			if (workaddress != -1) {
				CopyUnicodeString(buffer + 2*pos, pbk->Entries[workaddress].Text);
				pos += UnicodeLength(pbk->Entries[workaddress].Text);
			}
			buffer[2*pos] = 0;
			buffer[2*pos + 1] = ';';
			pos++;
			if (workcity != -1) {
				CopyUnicodeString(buffer + 2*pos, pbk->Entries[workcity].Text);
				pos += UnicodeLength(pbk->Entries[workcity].Text);
			}
			buffer[2*pos] = 0;
			buffer[2*pos + 1] = ';';
			pos++;
			if (workstate != -1) {
				CopyUnicodeString(buffer + 2*pos, pbk->Entries[workstate].Text);
				pos += UnicodeLength(pbk->Entries[workstate].Text);
			}
			buffer[2*pos] = 0;
			buffer[2*pos + 1] = ';';
			pos++;
			if (workzip != -1) {
				CopyUnicodeString(buffer + 2*pos, pbk->Entries[workzip].Text);
				pos += UnicodeLength(pbk->Entries[workzip].Text);
			}
			buffer[2*pos] = 0;
			buffer[2*pos + 1] = ';';
			pos++;
			if (workcountry != -1) {
				CopyUnicodeString(buffer + 2*pos, pbk->Entries[workcountry].Text);
				pos += UnicodeLength(pbk->Entries[workcountry].Text);
			}
			buffer[2*pos] = 0;
			buffer[2*pos + 1] = 0;
			error = VC_StoreText(Buffer, buff_len, Length, buffer, "ADR;WORK", false);
			if (error != ERR_NONE) return error;
		}
		/* Save address if it is composed from parts */
		if (address != -1 || city != -1 || state != -1 || zip != -1 || country != -1) {
			pos = 0;
			buffer[2*pos] = 0;
			buffer[2*pos + 1] = ';';
			pos++;
			buffer[2*pos] = 0;
			buffer[2*pos + 1] = ';';
			pos++;
			if (address != -1) {
				CopyUnicodeString(buffer + 2*pos, pbk->Entries[address].Text);
				pos += UnicodeLength(pbk->Entries[address].Text);
			}
			buffer[2*pos] = 0;
			buffer[2*pos + 1] = ';';
			pos++;
			if (city != -1) {
				CopyUnicodeString(buffer + 2*pos, pbk->Entries[city].Text);
				pos += UnicodeLength(pbk->Entries[city].Text);
			}
			buffer[2*pos] = 0;
			buffer[2*pos + 1] = ';';
			pos++;
			if (state != -1) {
				CopyUnicodeString(buffer + 2*pos, pbk->Entries[state].Text);
				pos += UnicodeLength(pbk->Entries[state].Text);
			}
			buffer[2*pos] = 0;
			buffer[2*pos + 1] = ';';
			pos++;
			if (zip != -1) {
				CopyUnicodeString(buffer + 2*pos, pbk->Entries[zip].Text);
				pos += UnicodeLength(pbk->Entries[zip].Text);
			}
			buffer[2*pos] = 0;
			buffer[2*pos + 1] = ';';
			pos++;
			if (country != -1) {
				CopyUnicodeString(buffer + 2*pos, pbk->Entries[country].Text);
				pos += UnicodeLength(pbk->Entries[country].Text);
			}
			buffer[2*pos] = 0;
			buffer[2*pos + 1] = 0;
			error = VC_StoreText(Buffer, buff_len, Length, buffer, "ADR;HOME", false);
			if (error != ERR_NONE) return error;
		}
	} else {
		return ERR_NOTSUPPORTED;
	}
	if (header) {
		error = VC_StoreLine(Buffer, buff_len, Length, "END:VCARD");
		if (error != ERR_NONE) return error;
	}
	return ERR_NONE;
}

void GSM_TweakInternationalNumber(unsigned char *Number, const GSM_NumberType numType)
{
	/* Checks if International number needs to be corrected */
	char* pos; /* current position in the buffer */
	char buf[500]; /* Taken from DecodeUnicodeString(). How to get length of the encoded unicode string? There may be embedded 0s. */

	if (numType == NUMBER_INTERNATIONAL_NUMBERING_PLAN_ISDN || numType + 1 == NUMBER_INTERNATIONAL_NUMBERING_PLAN_ISDN) {
		sprintf(buf+1,"%s",DecodeUnicodeString(Number)); /* leave 1 free char before the number, we'll need it */
		/* International number may be without + (e.g. (Sony)Ericsson)
			we can add it, but must handle numbers in the form:
			         NNNNNN         N - any digit (char)
			   *code#NNNNNN         any number of Ns
			*[*]code*NNNNNN[...]
			other combinations (like *code1*code2*number#)
			will have to be added if found in real life
			Or does somebody know the exact allowed syntax
			from some standard?
		*/
		pos=buf+1;
		if (*pos=='*') { /* Code? Skip it. */
			/* probably with code */
			while (*pos=='*') { /* skip leading asterisks */
				*(pos-1)=*pos; /* shift the chars by one */
				pos++;
			}
			while ((*pos!='*')&&(*pos!='#')) { /* skip code - anything except * or # */
				*(pos-1)=*pos;
				pos++;
			}
			*(pos-1)=*pos; /* shift the last delimiter */
			pos++;
	        }
		/* check the guessed location, if + is correctly there */
		if (*pos=='+') {
			/* yes, just shift the rest of the string */
			while (*pos) {
				*(pos-1) = *pos;
				pos++;
			}
			*(pos-1)=0; /* kill the last char, which now got doubled */
		} else {
			/* no, insert + and exit, no more shifting */
			*(pos-1)='+';
		}
		EncodeUnicode(Number,buf,strlen(buf));
	}
}


#define CHECK_NUM_ENTRIES if (Pbk->EntriesNum >= GSM_PHONEBOOK_ENTRIES) { error = ERR_MOREMEMORY; goto vcard_done; }

/**
 * \bug We should avoid using static buffers here.
 */
GSM_Error GSM_DecodeVCARD(char *Buffer, size_t *Pos, GSM_MemoryEntry *Pbk, GSM_VCardVersion Version UNUSED)
{
	char   Buff[20000];
	int	     Level = 0;
	char   *s;
	int		pos;
	GSM_Error	error;
	char	*Line = NULL;

	Buff[0]	 = 0;
	Pbk->EntriesNum = 0;

	while (1) {
		free(Line);
		Line = NULL;
		error = GSM_GetVCSLine(&Line, Buffer, Pos, strlen(Buffer), true);
		if (error != ERR_NONE) goto vcard_done;
		if (strlen(Line) == 0) break;
		switch (Level) {
		case 0:
			if (strstr(Line,"BEGIN:VCARD")) Level = 1;
			break;
		case 1:
			CHECK_NUM_ENTRIES;
			if (strstr(Line,"END:VCARD")) {
				goto vcard_complete;
			}
			Pbk->Entries[Pbk->EntriesNum].AddError = ERR_NONE;
			if (ReadVCALText(Line, "N", Buff, false)) {
				pos = 0;
				s = VCALGetTextPart(Buff, &pos);
				if (s == NULL) {
					CopyUnicodeString(Pbk->Entries[Pbk->EntriesNum].Text,Buff);
					Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Text_Name;
					Pbk->EntriesNum++;
				} else {
					CopyUnicodeString(Pbk->Entries[Pbk->EntriesNum].Text, s);
					/* Skip empty name */
					if (UnicodeLength(Pbk->Entries[Pbk->EntriesNum].Text) > 0) {
						Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Text_LastName;
						Pbk->EntriesNum++;
					}

					s = VCALGetTextPart(Buff, &pos);
					if (s == NULL) continue;
					CHECK_NUM_ENTRIES;
					CopyUnicodeString(Pbk->Entries[Pbk->EntriesNum].Text, s);
					Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Text_FirstName;
					Pbk->EntriesNum++;
				}
			}
			if (strncmp(Line, "PHOTO;JPEG;BASE64:", 18) == 0 ||
				strncmp(Line, "PHOTO;BASE64;JPEG:", 18) == 0 ||
				strncmp(Line, "PHOTO;TYPE=JPEG;BASE64:", 23) == 0 ||
				strncmp(Line, "PHOTO;BASE64;TYPE=JPEG:", 23) == 0 ||
				strncmp(Line, "PHOTO;TYPE=JPEG;ENCODING=BASE64:", 32) == 0 ||
				strncmp(Line, "PHOTO;ENCODING=BASE64;TYPE=JPEG:", 32) == 0 ||
				strncmp(Line, "PHOTO;JPEG;ENCODING=BASE64:", 27) == 0 ||
				strncmp(Line, "PHOTO;ENCODING=BASE64;JPEG:", 27) == 0) {
				/* Find : (it should be there we matched it above) */
				s = strchr(Line, ':');
				s++;
				/* Skip whitespace */
				while (isspace(*s) && *s) s++;

				Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Photo;
				Pbk->Entries[Pbk->EntriesNum].Picture.Type = PICTURE_JPG;

				/* We allocate here more memory than is actually required */
				Pbk->Entries[Pbk->EntriesNum].Picture.Buffer = malloc(strlen(s));
				if (Pbk->Entries[Pbk->EntriesNum].Picture.Buffer == NULL)
					return ERR_MOREMEMORY;

				Pbk->Entries[Pbk->EntriesNum].Picture.Length =
					DecodeBASE64(s, Pbk->Entries[Pbk->EntriesNum].Picture.Buffer, strlen(s));
				Pbk->EntriesNum++;
			}

			if (ReadVCALText(Line, "TEL",		   Buff, false) ||
			    ReadVCALText(Line, "TEL;VOICE",	     Buff, false) ||
			    ReadVCALText(Line, "TEL;TYPE=VOICE",	     Buff, false)) {
				if (Buff[1] == '+') {
					GSM_TweakInternationalNumber(Buff, NUMBER_INTERNATIONAL_NUMBERING_PLAN_ISDN);
				}
				CopyUnicodeString(Pbk->Entries[Pbk->EntriesNum].Text,Buff);
				Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Number_General;
				Pbk->Entries[Pbk->EntriesNum].SMSList[0] = 0;
				Pbk->Entries[Pbk->EntriesNum].VoiceTag = 0;
				Pbk->EntriesNum++;
			}
			if (ReadVCALText(Line, "TEL;CELL",	      Buff, false) ||
			    ReadVCALText(Line, "TEL;CELL;VOICE",	Buff, false) ||
			    ReadVCALText(Line, "TEL;TYPE=CELL",	 Buff, false) ||
			    ReadVCALText(Line, "TEL;TYPE=CELL,VOICE",   Buff, false) ||
			    ReadVCALText(Line, "TEL;TYPE=CELL;TYPE=VOICE",   Buff, false) ||
			    ReadVCALText(Line, "TEL;TYPE=CELL;VOICE",   Buff, false)) {
				if (Buff[1] == '+') {
					GSM_TweakInternationalNumber(Buff, NUMBER_INTERNATIONAL_NUMBERING_PLAN_ISDN);
				}
				CopyUnicodeString(Pbk->Entries[Pbk->EntriesNum].Text,Buff);
				Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Number_Mobile;
				Pbk->Entries[Pbk->EntriesNum].SMSList[0] = 0;
				Pbk->Entries[Pbk->EntriesNum].VoiceTag = 0;
				Pbk->EntriesNum++;
			}
			if (ReadVCALText(Line, "TEL;WORK",	      Buff, false) ||
			    ReadVCALText(Line, "TEL;TYPE=WORK",	 Buff, false) ||
			    ReadVCALText(Line, "TEL;WORK;VOICE",	Buff, false) ||
			    ReadVCALText(Line, "TEL;TYPE=WORK;VOICE",   Buff, false) ||
			    ReadVCALText(Line, "TEL;TYPE=WORK;TYPE=VOICE",   Buff, false) ||
			    ReadVCALText(Line, "TEL;TYPE=WORK,VOICE",   Buff, false)) {
				if (Buff[1] == '+') {
					GSM_TweakInternationalNumber(Buff, NUMBER_INTERNATIONAL_NUMBERING_PLAN_ISDN);
				}
				CopyUnicodeString(Pbk->Entries[Pbk->EntriesNum].Text,Buff);
				Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Number_Work;
				Pbk->Entries[Pbk->EntriesNum].SMSList[0] = 0;
				Pbk->Entries[Pbk->EntriesNum].VoiceTag = 0;
				Pbk->EntriesNum++;
			}
			if (ReadVCALText(Line, "TEL;OTHER",	      Buff, false) ||
			    ReadVCALText(Line, "TEL;TYPE=OTHER",	 Buff, false) ||
			    ReadVCALText(Line, "TEL;OTHER;VOICE",	Buff, false) ||
			    ReadVCALText(Line, "TEL;TYPE=OTHER;VOICE",   Buff, false) ||
			    ReadVCALText(Line, "TEL;TYPE=OTHER;TYPE=VOICE",   Buff, false) ||
			    ReadVCALText(Line, "TEL;TYPE=OTHER,VOICE",   Buff, false)) {
				if (Buff[1] == '+') {
					GSM_TweakInternationalNumber(Buff, NUMBER_INTERNATIONAL_NUMBERING_PLAN_ISDN);
				}
				CopyUnicodeString(Pbk->Entries[Pbk->EntriesNum].Text,Buff);
				Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Number_Other;
				Pbk->Entries[Pbk->EntriesNum].SMSList[0] = 0;
				Pbk->Entries[Pbk->EntriesNum].VoiceTag = 0;
				Pbk->EntriesNum++;
			}
			if (ReadVCALText(Line, "TEL;PAGER",	      Buff, false) ||
			    ReadVCALText(Line, "TEL;TYPE=PAGER",	 Buff, false) ||
			    ReadVCALText(Line, "TEL;PAGER;VOICE",	Buff, false) ||
			    ReadVCALText(Line, "TEL;TYPE=PAGER;VOICE",   Buff, false) ||
			    ReadVCALText(Line, "TEL;TYPE=PAGER;TYPE=VOICE",   Buff, false) ||
			    ReadVCALText(Line, "TEL;TYPE=PAGER,VOICE",   Buff, false)) {
				if (Buff[1] == '+') {
					GSM_TweakInternationalNumber(Buff, NUMBER_INTERNATIONAL_NUMBERING_PLAN_ISDN);
				}
				CopyUnicodeString(Pbk->Entries[Pbk->EntriesNum].Text,Buff);
				Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Number_Other;
				Pbk->Entries[Pbk->EntriesNum].SMSList[0] = 0;
				Pbk->Entries[Pbk->EntriesNum].VoiceTag = 0;
				Pbk->EntriesNum++;
			}
			if (ReadVCALText(Line, "TEL;MSG",	      Buff, false) ||
			    ReadVCALText(Line, "TEL;TYPE=MSG",	 Buff, false) ||
			    ReadVCALText(Line, "TEL;MSG;VOICE",	Buff, false) ||
			    ReadVCALText(Line, "TEL;TYPE=MSG;VOICE",   Buff, false) ||
			    ReadVCALText(Line, "TEL;TYPE=MSG;TYPE=VOICE",   Buff, false) ||
			    ReadVCALText(Line, "TEL;TYPE=MSG,VOICE",   Buff, false)) {
				if (Buff[1] == '+') {
					GSM_TweakInternationalNumber(Buff, NUMBER_INTERNATIONAL_NUMBERING_PLAN_ISDN);
				}
				CopyUnicodeString(Pbk->Entries[Pbk->EntriesNum].Text,Buff);
				Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Number_Other;
				Pbk->Entries[Pbk->EntriesNum].SMSList[0] = 0;
				Pbk->Entries[Pbk->EntriesNum].VoiceTag = 0;
				Pbk->EntriesNum++;
			}
			/* FAX + VOICE looks like nonsense */
			if (ReadVCALText(Line, "TEL;FAX",	       Buff, false) ||
			    ReadVCALText(Line, "TEL;TYPE=FAX",	  Buff, false) ||
			    ReadVCALText(Line, "TEL;FAX;VOICE",	 Buff, false) ||
			    ReadVCALText(Line, "TEL;TYPE=FAX;VOICE",    Buff, false) ||
			    ReadVCALText(Line, "TEL;TYPE=FAX;TYPE=VOICE",    Buff, false) ||
			    ReadVCALText(Line, "TEL;TYPE=FAX,VOICE",    Buff, false)) {
				if (Buff[1] == '+') {
					GSM_TweakInternationalNumber(Buff, NUMBER_INTERNATIONAL_NUMBERING_PLAN_ISDN);
				}
				CopyUnicodeString(Pbk->Entries[Pbk->EntriesNum].Text,Buff);
				Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Number_Fax;
				Pbk->Entries[Pbk->EntriesNum].SMSList[0] = 0;
				Pbk->Entries[Pbk->EntriesNum].VoiceTag = 0;
				Pbk->EntriesNum++;
			}
			if (ReadVCALText(Line, "TEL;HOME",	      Buff, false) ||
			    ReadVCALText(Line, "TEL;TYPE=HOME",	 Buff, false) ||
			    ReadVCALText(Line, "TEL;HOME;VOICE",	Buff, false) ||
			    ReadVCALText(Line, "TEL;TYPE=HOME,VOICE",   Buff, false) ||
			    ReadVCALText(Line, "TEL;TYPE=HOME;TYPE=VOICE",   Buff, false) ||
			    ReadVCALText(Line, "TEL;TYPE=HOME;VOICE",   Buff, false)) {
				if (Buff[1] == '+') {
					GSM_TweakInternationalNumber(Buff, NUMBER_INTERNATIONAL_NUMBERING_PLAN_ISDN);
				}
				CopyUnicodeString(Pbk->Entries[Pbk->EntriesNum].Text,Buff);
				Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Number_Home;
				Pbk->Entries[Pbk->EntriesNum].SMSList[0] = 0;
				Pbk->Entries[Pbk->EntriesNum].VoiceTag = 0;
				Pbk->EntriesNum++;
			}
			if (ReadVCALText(Line, "TITLE", Buff, false)) {
				CopyUnicodeString(Pbk->Entries[Pbk->EntriesNum].Text,Buff);
				Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Text_JobTitle;
				Pbk->EntriesNum++;
			}
			if (ReadVCALText(Line, "NOTE", Buff, false)) {
				CopyUnicodeString(Pbk->Entries[Pbk->EntriesNum].Text,Buff);
				Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Text_Note;
				Pbk->EntriesNum++;
			}
			if (ReadVCALText(Line, "LABEL", Buff, false) ||
			    ReadVCALText(Line, "ADR", Buff, false) ||
			    ReadVCALText(Line, "ADR;HOME", Buff, false)) {
				pos = 0;
				s = VCALGetTextPart(Buff, &pos); /* PO box, ignore for now */
				if (s == NULL) {
					CopyUnicodeString(Pbk->Entries[Pbk->EntriesNum].Text,Buff);
					Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Text_Postal;
					Pbk->EntriesNum++;
				} else {
					s = VCALGetTextPart(Buff, &pos); /* Don't know ... */

					s = VCALGetTextPart(Buff, &pos);
					if (s == NULL) continue;
					CopyUnicodeString(Pbk->Entries[Pbk->EntriesNum].Text, s);
					Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Text_StreetAddress;
					Pbk->EntriesNum++;
					CHECK_NUM_ENTRIES;

					s = VCALGetTextPart(Buff, &pos);
					if (s == NULL) continue;
					CopyUnicodeString(Pbk->Entries[Pbk->EntriesNum].Text, s);
					Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Text_City;
					Pbk->EntriesNum++;
					CHECK_NUM_ENTRIES;

					s = VCALGetTextPart(Buff, &pos);
					if (s == NULL) continue;
					CopyUnicodeString(Pbk->Entries[Pbk->EntriesNum].Text, s);
					Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Text_State;
					Pbk->EntriesNum++;
					CHECK_NUM_ENTRIES;

					s = VCALGetTextPart(Buff, &pos);
					if (s == NULL) continue;
					CopyUnicodeString(Pbk->Entries[Pbk->EntriesNum].Text, s);
					Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Text_Zip;
					Pbk->EntriesNum++;
					CHECK_NUM_ENTRIES;

					s = VCALGetTextPart(Buff, &pos);
					if (s == NULL) continue;
					CopyUnicodeString(Pbk->Entries[Pbk->EntriesNum].Text, s);
					Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Text_Country;
					Pbk->EntriesNum++;
				}
			}
			if (ReadVCALText(Line, "ADR;WORK", Buff, false)) {
				pos = 0;
				s = VCALGetTextPart(Buff, &pos); /* PO box, ignore for now */
				if (s == NULL) {
					CopyUnicodeString(Pbk->Entries[Pbk->EntriesNum].Text,Buff);
					Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Text_WorkPostal;
					Pbk->EntriesNum++;
				} else {
					s = VCALGetTextPart(Buff, &pos); /* Don't know ... */

					s = VCALGetTextPart(Buff, &pos);
					if (s == NULL) continue;
					CopyUnicodeString(Pbk->Entries[Pbk->EntriesNum].Text, s);
					Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Text_WorkStreetAddress;
					Pbk->EntriesNum++;
					CHECK_NUM_ENTRIES;

					s = VCALGetTextPart(Buff, &pos);
					if (s == NULL) continue;
					CopyUnicodeString(Pbk->Entries[Pbk->EntriesNum].Text, s);
					Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Text_WorkCity;
					Pbk->EntriesNum++;
					CHECK_NUM_ENTRIES;

					s = VCALGetTextPart(Buff, &pos);
					if (s == NULL) continue;
					CopyUnicodeString(Pbk->Entries[Pbk->EntriesNum].Text, s);
					Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Text_WorkState;
					Pbk->EntriesNum++;
					CHECK_NUM_ENTRIES;

					s = VCALGetTextPart(Buff, &pos);
					if (s == NULL) continue;
					CopyUnicodeString(Pbk->Entries[Pbk->EntriesNum].Text, s);
					Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Text_WorkZip;
					Pbk->EntriesNum++;
					CHECK_NUM_ENTRIES;

					s = VCALGetTextPart(Buff, &pos);
					if (s == NULL) continue;
					CopyUnicodeString(Pbk->Entries[Pbk->EntriesNum].Text, s);
					Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Text_WorkCountry;
					Pbk->EntriesNum++;
				}
			}
			if (ReadVCALText(Line, "EMAIL", Buff, false) ||
			    ReadVCALText(Line, "EMAIL;TYPE=OTHER", Buff, false) ||
			    ReadVCALText(Line, "EMAIL;INTERNET", Buff, false)) {
				CopyUnicodeString(Pbk->Entries[Pbk->EntriesNum].Text,Buff);
				Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Text_Email;
				Pbk->EntriesNum++;
			}
			if (ReadVCALText(Line, "X-IRMC-LUID", Buff, false)) {
				CopyUnicodeString(Pbk->Entries[Pbk->EntriesNum].Text,Buff);
				Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Text_LUID;
				Pbk->EntriesNum++;
			}
			if (ReadVCALText(Line, "URL", Buff, false)) {
				CopyUnicodeString(Pbk->Entries[Pbk->EntriesNum].Text,Buff);
				Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Text_URL;
				Pbk->EntriesNum++;
			}
			if (ReadVCALText(Line, "ORG", Buff, false)) {
				CopyUnicodeString(Pbk->Entries[Pbk->EntriesNum].Text,Buff);
				Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Text_Company;
				Pbk->EntriesNum++;
			}
			if (ReadVCALText(Line, "NICKNAME", Buff, false)) {
				CopyUnicodeString(Pbk->Entries[Pbk->EntriesNum].Text,Buff);
				Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Text_NickName;
				Pbk->EntriesNum++;
			}
			if (ReadVCALText(Line, "FN", Buff, false)) {
				CopyUnicodeString(Pbk->Entries[Pbk->EntriesNum].Text,Buff);
				Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Text_FormalName;
				Pbk->EntriesNum++;
			}
			if (ReadVCALText(Line, "CATEGORIES", Buff, false)) {
				CopyUnicodeString(Pbk->Entries[Pbk->EntriesNum].Text,Buff);
				Pbk->Entries[Pbk->EntriesNum].Number = -1;
				Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Category;
				Pbk->EntriesNum++;
			}
			if (ReadVCALText(Line, "BDAY", Buff, false)) {
				if (ReadVCALDateTime(DecodeUnicodeString(Buff), &Pbk->Entries[Pbk->EntriesNum].Date)) {
					Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Date;
					Pbk->EntriesNum++;
				}
			}
			if (ReadVCALText(Line, "LAST-MODIFIED", Buff, false)) {
				if (ReadVCALDateTime(DecodeUnicodeString(Buff), &Pbk->Entries[Pbk->EntriesNum].Date)) {
					Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_LastModified;
					Pbk->EntriesNum++;
				}
			}
			if (ReadVCALText(Line, "X-PRIVATE", Buff, false)) {
				Pbk->Entries[Pbk->EntriesNum].Number = atoi(DecodeUnicodeString(Buff));
				Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Private;
				Pbk->EntriesNum++;
			}
			if (ReadVCALText(Line, "X-CALLER-GROUP", Buff, false)) {
				Pbk->Entries[Pbk->EntriesNum].Number = atoi(DecodeUnicodeString(Buff));
				Pbk->Entries[Pbk->EntriesNum].EntryType = PBK_Caller_Group;
				Pbk->EntriesNum++;
			}
			if (ReadVCALText(Line, "X-GAMMU-LOCATION", Buff, false)) {
				Pbk->Location = atoi(DecodeUnicodeString(Buff));
			}
			if (ReadVCALText(Line, "X-GAMMU-MEMORY", Buff, false)) {
				Pbk->MemoryType = GSM_StringToMemoryType(DecodeUnicodeString(Buff));
			}
			break;
		}
	}

vcard_complete:
	if (Pbk->EntriesNum == 0) error = ERR_EMPTY;
	else error = ERR_NONE;

vcard_done:
	free(Line);
	return error;
}

/* How should editor hadle tabs in this file? Add editor commands here.
 * vim: noexpandtab sw=4 ts=4 sts=4 tw=79:
 */
