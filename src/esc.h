

template<class T> std::string esc(const T &data)
{
    std::string out = "";
    for (auto ch: data)
    {
        if (ch < 0x20)
        {
            char tmp[1024]; 
            sprintf(tmp, "\\x%02x", ch);
            out += tmp;
            continue;
        }
        if (isprint(ch))
            out += ch;
    }
    return out;
}


