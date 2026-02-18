cat > notes.md << 'EOF'
# Cscope + Ctags + Vim Navigation Notes

## 1. Install Required Tools

sudo apt update
sudo apt install cscope universal-ctags

---

# 2. Generate Databases (run in project root)

## Step 1 — Create file list for cscope

find . -name "*.[ch]" > cscope.files

For C++ projects:

find . -name "*.[ch]" -o -name "*.cpp" > cscope.files

---

## Step 2 — Build cscope database

cscope -Rbq

Generated files:

cscope.out
cscope.in.out
cscope.po.out

---

## Step 3 — Build tags database

ctags -R .

Generated file:

tags

---

# 3. Using Cscope Terminal UI

Start UI:

cscope

## Navigation Keys

↑ ↓    Move between options  
TAB    Move to input box  
ENTER  Search / Open result  
Ctrl+B Go back  
q      Exit pager  

---

# 4. Useful Searches in Cscope

Find this C symbol                  Search functions, structs, variables  
Find global definition              Jump to function definition  
Functions calling this function     Show callers  
Functions called by this function   Show called functions  
Find this text string               Text search  
Find this file                      Open file quickly  

---

# 5. Using Vim with Cscope + Ctags

Open file:

vim main.c

Add cscope database:

:cs add cscope.out

---

## Jump Commands in Vim

Ctrl+]           Jump to definition  
Ctrl+T           Go back to previous location  
:cs find g func  Go to global definition  
:cs find c func  Show callers  
:cs find d func  Show called functions  
:cs find t text  Search text  
gd               Go to local definition  
gD               Go to global definition  

---

# 6. Recommended .vimrc Setup

Add this to ~/.vimrc:

set tags=./tags;,tags

if filereadable("cscope.out")
    cs add cscope.out
endif

set cscopequickfix=s-,c-,d-,i-,t-,e-

nnoremap <C-]> :cs find g <C-R>=expand("<cword>")<CR><CR>

---

# 7. Rebuild Databases After Code Change

find . -name "*.[ch]" > cscope.files
cscope -Rbq
ctags -R .

---

# 8. Typical Workflow

1. Build databases once
2. Open Vim
3. Ctrl+] to jump to function
4. Ctrl+T to go back
5. Use :cs find c func to see callers
6. Follow execution across files

Done.
EOF
