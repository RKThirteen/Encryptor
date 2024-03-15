# Encryptor
Very basic encryption that takes a textfile and encrypts it with the option to decrypt

# Compiled and ran in a Linux environment

# To encrypt: ./encryptor [file_to_encrypt]

Let's say the file you give it is called "file". After encryption, you will receive the files "file_encrypted" and "perms"
file_encrypted--> your words are now random permutations of themselves
perms --> stores what swaps have been made in each word

# To decrypt: ./decryptor [your encrypted file] perms

You use the perms file which stored your permutations in order to decrypt your file back to what it used to be.
Note: Your decrypted file will be named "file_encrypted_decrypted"



